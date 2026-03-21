//
// Created by Joseph Alai on 7/6/21.
//

#include "MasterRenderer.h"
#include "DisplayManager.h"
#include "RenderStyle.h"
#include "AssimpEntityLoader.h"
#include "../Util/ColorName.h"
#include "../OpenGLWrapper/OpenGLUtils.h"

MasterRenderer::MasterRenderer(PlayerCamera *cameraInput, Loader *loader) : shader(new StaticShader()),
                                                            renderer(new EntityRenderer(shader)),
                                                            camera(cameraInput), projectionMatrix(
                Maths::createProjectionMatrix(masterFovy(), static_cast<float>(DisplayManager::Width()),
                                              static_cast<float>(DisplayManager::Height()), masterNearPlane(), masterFarPlane())),
                                                            terrainShader(new TerrainShader()),
                                                            sceneShader(new AssimpStaticShader()),
                                                            bShader(new BoundingBoxShader()){
    RenderStyle::enableCulling();
    entities = new std::map<TexturedModel *, std::vector<Entity *>>;
    scenes = new std::map<AssimpMesh *, std::vector<AssimpModelComponent>>;
    terrains = new std::vector<Terrain *>;
    boxes = new std::map<RawBoundingBox *, std::vector<Entity *>>;
    staticBatches = new std::map<TexturedModel*, std::vector<EntityRenderer::RenderData>>;
    terrainRenderer = new TerrainRenderer(terrainShader, this->projectionMatrix);
    sceneRenderer = new AssimpEntityRenderer(sceneShader);
    skyboxRenderer = new SkyboxRenderer(loader, this->projectionMatrix, &skyColor);
    bRenderer = new BoundingBoxRenderer(bShader, this->projectionMatrix);

    // PBR shader — initialise lazily so OpenGL context is guaranteed
    pbrShader = new PBRShader();

    // Instanced rendering
    instancedShader   = new InstancedShader();
    instancedRenderer = new InstancedRenderer(instancedShader);

    // Apply environment preset from ConfigManager (data-driven sky/fog).
    const auto& env = ConfigManager::get().environment.current();
    skyColor = Color(env.skyColor);
    fogSettings.density = env.fogDensity;
    fogSettings.start   = env.fogStart;
    fogSettings.end     = env.fogEnd;
    fogSettings.color   = env.fogColor;
}

void MasterRenderer::cleanUp() {
    shader->cleanUp();
    terrainShader->cleanUp();
    sceneShader->cleanUp();
    bShader->cleanUp();
    if (pbrShader)         pbrShader->cleanUp();
    if (shadowShader)      shadowShader->cleanUp();
    if (waterRenderer)     waterRenderer->cleanUp();
    if (instancedShader)   instancedShader->cleanUp();
}

Color MasterRenderer::skyColor = const_cast<Color &>(ColorName::Skyblue);

/**
 * @brief prepares and clears buffer and screen for each iteration of loop
 */
void MasterRenderer::prepare() {
    // render
    // ------
    shader->loadSkyColorVariable(skyColor);
    terrainShader->loadSkyColorVariable(skyColor);
    OpenGLUtils::clearFrameBuffer(skyColor);

}

void MasterRenderer::render(const std::vector<Light *>&suns) {
    this->prepare();

    shader->start();

    shader->loadSkyColorVariable(skyColor);
    shader->loadLight(suns);
    shader->loadViewPosition(camera);
    shader->loadViewMatrix(camera->getViewMatrix());
    shader->loadProjectionMatrix(MasterRenderer::createProjectionMatrix());
    shader->loadFogDensity(fogSettings.density);
    renderer->render(entities);
    // Also render ECS static entities batched without Entity*
    renderer->renderStaticBatch(staticBatches);

    entities->clear();
    staticBatches->clear();
    shader->stop();

    sceneShader->start();
    sceneShader->loadSkyColorVariable(skyColor);
    sceneShader->loadLight(suns);
    sceneShader->loadViewPosition(camera);
    sceneShader->loadViewMatrix(camera->getViewMatrix());
    sceneShader->loadProjectionMatrix(MasterRenderer::createProjectionMatrix());
    sceneRenderer->render(scenes);

    scenes->clear();
    sceneShader->stop();


    terrainShader->start();

    terrainShader->loadSkyColorVariable(skyColor);
    terrainShader->loadLight(suns);
    terrainShader->loadViewPosition(camera);
    terrainShader->loadViewMatrix(camera->getViewMatrix());
    terrainShader->loadProjectionMatrix(MasterRenderer::createProjectionMatrix());
    terrainShader->loadFogDensity(fogSettings.density);
    terrainRenderer->render(terrains);

    terrains->clear();
    terrainShader->stop();
    skyboxRenderer->render(camera);

}

void MasterRenderer::processTerrain(Terrain *terrain) {
    terrains->push_back(terrain);
}

glm::mat4 MasterRenderer::createProjectionMatrix() {
    // my additions
    return Maths::createProjectionMatrix(PlayerCamera::Zoom, static_cast<GLfloat>(DisplayManager::Width()),
                                         static_cast<GLfloat>(DisplayManager::Height()), masterNearPlane(),
                                         masterFarPlane());
}

glm::mat4 MasterRenderer::getProjectionMatrix() {
    return projectionMatrix;
}

void MasterRenderer::processEntity(Entity *entity) {
    TexturedModel *entityModel = entity->getModel();
    if (!entityModel) return;
    auto batchIterator = entities->find(entityModel);
    if (batchIterator != entities->end()) {
        batchIterator->second.push_back(entity);
    } else {
        std::vector<Entity *> newBatch;
        newBatch.push_back(entity);
        (*entities)[entityModel] = newBatch;
    }
}

void MasterRenderer::processStaticEntity(const TransformComponent& tc,
                                          const StaticModelComponent& smc) {
    if (!smc.model) return;
    // Compute texture atlas offsets from textureIndex
    int rows = smc.model->getModelTexture()->getNumberOfRows();
    float xOff = (rows > 0) ? static_cast<float>(smc.textureIndex % rows) / static_cast<float>(rows) : 0.0f;
    float yOff = (rows > 0) ? static_cast<float>(smc.textureIndex / rows) / static_cast<float>(rows) : 0.0f;
    EntityRenderer::RenderData data;
    data.position  = tc.position;
    data.rotation  = tc.rotation;
    data.scale     = tc.scale;
    data.texXOffset = xOff;
    data.texYOffset = yOff;
    data.material   = smc.model->getModelTexture()->getMaterial();
    (*staticBatches)[smc.model].push_back(data);
}

void MasterRenderer::processAssimpEntity(const AssimpModelComponent& comp) {
    AssimpMesh *model = comp.mesh;
    auto batchIterator = scenes->find(model);
    if (batchIterator != scenes->end()) {
        batchIterator->second.push_back(comp);
    } else {
        std::vector<AssimpModelComponent> newBatch;
        newBatch.push_back(comp);
        (*scenes)[model] = newBatch;
    }
}

void MasterRenderer::processBoundingBox(Entity *entity) {
    auto coloredBox = entity->getBoundingBox()->getRawBoundingBox();
    auto itBoxes = boxes->find(coloredBox);
    if (itBoxes != boxes->end()) {
        itBoxes->second.push_back(entity);
    } else {
        std::vector<Entity *> newBatch;
        newBatch.push_back(entity);
        (*boxes)[coloredBox] = newBatch;
    }
}

void MasterRenderer::renderScene(std::vector<Entity *> entities, std::vector<AssimpModelComponent> aComps,
                                 std::vector<Terrain *> terrains, std::vector<Light *> lights) {
    for (Terrain *ter : terrains) {
        processTerrain(ter);
    }

    for (Entity *ent : entities) {
        processEntity(ent);
    }

    for (const AssimpModelComponent& comp : aComps) {
        processAssimpEntity(comp);
    }

    render(lights);
}

/**
 * Renders Bounding Boxes
 *
 * Inputs a variety of Entity objects, which means each of them have bounding boxes. Then the bounding boxes
 * are rendered with each of their colors.
 *
 * @param boxes
 */
void MasterRenderer::renderBoundingBoxes(std::vector<Entity*> entities) {
    for (Entity *entity : entities) {
        if (entity->getBoundingBox() != nullptr) {
            processBoundingBox(entity);
        }
    }
    render();
}

void MasterRenderer::render() {
    this->prepareBoundingBoxRender();
    bShader->start();

    bShader->loadViewPosition(camera);
    bShader->loadViewMatrix(camera->getViewMatrix());
    bShader->loadProjectionMatrix(MasterRenderer::createProjectionMatrix());
    bRenderer->render(boxes);

    boxes->clear();
    bShader->stop();
}

/**
 * @brief prepares and clears buffer and screen for each iteration of loop
 */
void MasterRenderer::prepareBoundingBoxRender() {
    // render
    // ------
    OpenGLUtils::clearFrameBuffer(Color(1.0f));
}

// ---------------------------------------------------------------------------
// Shadow mapping
// ---------------------------------------------------------------------------

void MasterRenderer::enableShadowMapping(int mapSize) {
    delete shadowMap;
    delete shadowShader;
    shadowMap    = new ShadowMap(mapSize);
    shadowShader = new ShadowShader();
}

void MasterRenderer::renderShadowPass(const std::vector<Entity*>& allEntities,
                                       const std::vector<Light*>&  lights) {
    if (!shadowMap || !shadowShader) return;

    // Use the first directional light for the shadow map
    glm::vec3 lightDir(0.0f, -1.0f, 0.0f);
    for (auto* l : lights) {
        if (l->getLighting().constant < 0.0f) {
            lightDir = glm::normalize(l->getPosition());
            break;
        }
    }

    glm::vec3 viewCenter = camera->Position;
    glm::mat4 lsm = shadowMap->computeLightSpaceMatrix(lightDir, viewCenter, 300.0f);
    shadowMap->renderShadowMap(allEntities, lsm, shadowShader);
    shadowMap->unbind(DisplayManager::Width(), DisplayManager::Height());
}

void MasterRenderer::renderShadowPassFromRegistry(entt::registry&           registry,
                                                   Entity*                    player,
                                                   const std::vector<Light*>& lights) {
    if (!shadowMap || !shadowShader) return;

    glm::vec3 lightDir(0.0f, -1.0f, 0.0f);
    for (auto* l : lights) {
        if (l->getLighting().constant < 0.0f) {
            lightDir = glm::normalize(l->getPosition());
            break;
        }
    }

    glm::vec3 viewCenter = camera->Position;
    glm::mat4 lsm = shadowMap->computeLightSpaceMatrix(lightDir, viewCenter, 300.0f);
    shadowMap->renderShadowMapFromRegistry(registry, player, lsm, shadowShader);
    shadowMap->unbind(DisplayManager::Width(), DisplayManager::Height());
}

void MasterRenderer::renderSceneFromRegistry(entt::registry&                  registry,
                                              Entity*                          player,
                                              const std::vector<AssimpModelComponent>& aComps,
                                              const std::vector<Terrain*>&     terrains,
                                              const std::vector<Light*>&       lights) {
    // Terrains
    for (Terrain* ter : terrains) processTerrain(ter);

    // Player (still Entity*-based)
    if (player) processEntity(player);

    // Static ECS entities
    auto staticView = registry.view<StaticModelComponent, TransformComponent>();
    for (auto e : staticView) {
        const auto& smc = staticView.get<StaticModelComponent>(e);
        const auto& tc  = staticView.get<TransformComponent>(e);
        processStaticEntity(tc, smc);
    }

    // Assimp entities
    for (const auto& comp : aComps) processAssimpEntity(comp);

    render(lights);
}

void MasterRenderer::renderBoundingBoxesFromRegistry(entt::registry& /*registry*/, Entity* player) {
    // Only the Player participates in object picking; render its bounding box only.
    if (player && player->getBoundingBox()) {
        processBoundingBox(player);
    }
    render();
}

// ---------------------------------------------------------------------------
// Water
// ---------------------------------------------------------------------------

void MasterRenderer::renderWater(Camera* cam, Light* sun) {
    if (!waterRenderer || waterTiles.empty()) return;
    waterRenderer->render(waterTiles, cam, createProjectionMatrix(), sun);
}

// ---------------------------------------------------------------------------
// Scene Graph
// ---------------------------------------------------------------------------

void MasterRenderer::renderSceneGraph(SceneGraph& graph, std::vector<AssimpModelComponent> aComps,
                                       std::vector<Terrain*> terrains, std::vector<Light*> lights) {
    graph.update();
    std::vector<Entity*> sgEntities = graph.collectEntities();
    renderScene(sgEntities, aComps, terrains, lights);
}

// ---------------------------------------------------------------------------
// Instanced Rendering
// ---------------------------------------------------------------------------

void MasterRenderer::processInstancedEntity(InstancedModel* model,
                                              const std::vector<glm::mat4>& transforms) {
    for (const auto& t : transforms) {
        instancedRenderer->addInstance(model, t);
    }
}

void MasterRenderer::renderInstanced(const std::vector<Light*>& lights) {
    instancedRenderer->render(lights, camera, createProjectionMatrix(),
                               skyColor.getColorRGB());
}
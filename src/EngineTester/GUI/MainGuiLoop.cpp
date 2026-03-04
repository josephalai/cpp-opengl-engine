//
// Created by Joseph Alai on 3/9/22.
//

#include "MainGuiLoop.h"

#include "../../Util/FileSystem.h"
#include "../../Util/Utils.h"
#include "../../Util/LightUtil.h"
#include "../../RenderEngine/DisplayManager.h"
#include "../../RenderEngine/EntityRenderer.h"
#include "../../RenderEngine/ObjLoader.h"
#include "../../RenderEngine/MasterRenderer.h"
#include "../../Guis/Texture/GuiTexture.h"
#include "../../Guis/Texture/Rendering/GuiRenderer.h"
#include "../../Guis/Rect/Rendering/RectRenderer.h"
#include "../../Toolbox/MousePicker.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "../../Guis/Text/GUIText.h"
#include "../../Guis/Text/Rendering/FontRenderer.h"
#include "../../Util/ColorName.h"
#include "../../Guis/Text/Rendering/TextMaster.h"
#include "../../Toolbox/TerrainPicker.h"
#include "../../RenderEngine/FrameBuffers.h"
#include "../../Interaction/InteractiveModel.h"
#include "../../Guis/UiMaster.h"
#include "../../Guis/Constraints/UiPercentConstraint.h"
#include "../../Guis/Constraints/UiPixelConstraint.h"
#include "../../Guis/Constraints/UiNormalizedConstraint.h"
#include "../../Guis/Text/FontMeshCreator/FontNames.h"
#include "../../Engine/SceneLoader.h"
#include "../../Scene/SceneGraph.h"
#include "../../Scene/SceneNode.h"
#include "../../Water/WaterTile.h"
#include "../../Water/WaterRenderer.h"
#include "../../Water/WaterShader.h"
#include "../../Atmosphere/FogSettings.h"
#include "../../Shadows/ShadowMap.h"
#include "../../Shaders/PBRMaterial.h"
// Phase 2 — Animation & Instanced Rendering
#include "../../Animation/AnimationController.h"
// Phase 3 — Bullet Physics
#include "../../Physics/PhysicsSystem.h"
#include "../../RenderEngine/AnimatedRenderer.h"
#include "../../Shaders/AnimatedShader.h"
#include "../../RenderEngine/InstancedModel.h"
#include "../../RenderEngine/InstancedRenderer.h"
#include "../../Shaders/InstancedShader.h"

#include <thread>

void MainGuiLoop::main() {

    // Initialise Display
    DisplayManager::createDisplay();

    // Initialize VAO / VBO Loader
    auto loader = new Loader();

    /**
     * Scene vectors populated by SceneLoader
     */
    std::vector<Entity *>       entities;
    std::vector<AssimpEntity *> assimpEntities;
    std::vector<Light *>        lights;
    std::vector<Terrain *>      allTerrains;
    std::vector<GuiTexture *>   guis;
    std::vector<GUIText *>      texts;
    std::vector<WaterTile>      waterTiles;
    std::vector<AnimatedEntity*> animatedEntities;
    Terrain*                    primaryTerrain = nullptr;
    Player*                     player         = nullptr;
    PlayerCamera*               playerCamera   = nullptr;

    // TextMaster must be initialized before SceneLoader::load() so that
    // GUIText constructors (which call TextMaster::loadText) work correctly.
    TextMaster::init(loader);

    /**
     * Load scene from scene.cfg (falls back to minimal defaults if file missing)
     */
    std::vector<SceneLoader::PhysicsBodyCfg>   physicsBodyCfgs;
    std::vector<SceneLoader::PhysicsGroundCfg> physicsGroundCfgs;
    bool sceneLoaded = SceneLoader::load(
        FileSystem::Scene("scene.cfg"),
        loader,
        entities, assimpEntities, lights, allTerrains, guis, texts, waterTiles,
        primaryTerrain, player, playerCamera,
        animatedEntities,
        physicsBodyCfgs, physicsGroundCfgs);

    // --- Phase 3: Physics world ------------------------------------------------
    // Always create the physics system, even when the fallback player is used.
    auto physicsSystem = new PhysicsSystem();
    physicsSystem->init();

    // Register ground planes from scene.cfg
    for (const auto& g : physicsGroundCfgs) {
        physicsSystem->addGroundPlane(g.yHeight);
    }

    // Register static/dynamic/kinematic rigid bodies from scene.cfg
    for (const auto& cfg : physicsBodyCfgs) {
        if (cfg.entityIndex < 0 || cfg.entityIndex >= static_cast<int>(entities.size()))
            continue;
        Entity* ent = entities[static_cast<size_t>(cfg.entityIndex)];
        if (!ent) continue;
        PhysicsBodyDef def;
        def.type        = cfg.type;
        def.shape       = cfg.shape;
        def.mass        = cfg.mass;
        def.halfExtents = cfg.halfExtents;
        def.radius      = cfg.radius;
        def.height      = cfg.height;
        def.friction    = cfg.friction;
        def.restitution = cfg.restitution;
        switch (def.type) {
            case BodyType::Static:
                physicsSystem->addStaticBody(ent, def.shape, def.halfExtents,
                                             def.friction, def.restitution);
                break;
            case BodyType::Kinematic:
                physicsSystem->addKinematicBody(ent, def);
                break;
            case BodyType::Dynamic:
            default:
                physicsSystem->addDynamicBody(ent, def);
                break;
        }
    }
    // ---------------------------------------------------------------------------

    if (!sceneLoaded || !player || !playerCamera) {
        std::cerr << "[MainGuiLoop] SceneLoader failed or missing player — using minimal defaults\n";

        auto backgroundTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/grass")->getId());
        auto rTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/dirt")->getId());
        auto gTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blueflowers")->getId());
        auto bTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/brickroad")->getId());
        auto texturePack = new TerrainTexturePack(backgroundTexture, rTexture, gTexture, bTexture);
        auto blendMap = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blendMap")->getId());
        primaryTerrain = new Terrain(0, -1, loader, texturePack, blendMap, "heightMap");
        allTerrains.push_back(primaryTerrain);

        lights.push_back(new Light(glm::vec3(0.0, 1000., -7000.0f), glm::vec3(0.4f, 0.4f, 0.4f), {
            .ambient  = glm::vec3(0.2f, 0.2f, 0.2f),
            .diffuse  = glm::vec3(0.5f, 0.5f, 0.5f),
            .constant = Light::kDirectional,
        }));

        ModelData stallData = OBJLoader::loadObjModel("Stall");
        if (!stallData.getIndices().empty()) {
            BoundingBoxData stallBbData = OBJLoader::loadBoundingBox(stallData, ClickBoxTypes::BOX, BoundTypes::AABB);
            RawBoundingBox* pStallBox = loader->loadToVAO(stallBbData);
            auto stallModel = new TexturedModel(loader->loadToVAO(stallData),
                                                new ModelTexture("stallTexture", PNG, Material{2.0f, 2.0f}));
            player = new Player(stallModel,
                                new BoundingBox(pStallBox, BoundingBoxIndex::genUniqueId()),
                                glm::vec3(100.0f, 3.0f, -50.0f),
                                glm::vec3(0.0f, 180.0f, 0.0f), 1.0f);
            InteractiveModel::setInteractiveBox(player);
            entities.push_back(player);
            playerCamera = new PlayerCamera(player);
        } else {
            std::cerr << "[MainGuiLoop] Could not load Stall model — resources path may be wrong\n";
        }
    }

    // Wire the player to the physics system (works whether it came from SceneLoader
    // or the fallback block above).
    // Register heightfield colliders for all terrain tiles (after the fallback block
    // so that any fallback terrain is also included).
    for (auto* t : allTerrains) {
        physicsSystem->addTerrainCollider(t);
    }
    if (player) {
        physicsSystem->setCharacterController(player, 1.0f, 3.0f);
        player->setPhysicsSystem(physicsSystem);
    }

    /**
     * -----------------------------------------------------------------------
     * Scene Graph (1.2) — wrap flat entities into a hierarchy.
     * -----------------------------------------------------------------------
     */
    SceneGraph sceneGraph;
    SceneNode* root = sceneGraph.getRoot();

    for (Entity* e : entities) {
        auto node = std::make_unique<SceneNode>("entity");
        node->localPosition = e->getPosition();
        node->localRotation = e->getRotation();
        node->localScale    = e->getScale();
        node->entity        = e;
        root->addChild(std::move(node));
    }

    // Demonstrate parent/child attachment on the player node
    if (player) {
        SceneNode* playerNode = sceneGraph.findNode("entity");
        if (playerNode) {
            auto attachment = std::make_unique<SceneNode>("playerAttachment");
            attachment->localPosition = glm::vec3(0.5f, 1.0f, 0.0f);
            attachment->entity        = nullptr;
            playerNode->addChild(std::move(attachment));
        }
    }

    /**
     * -----------------------------------------------------------------------
     * Renderers
     * -----------------------------------------------------------------------
     */
    auto fontRenderer = new FontRenderer();
    auto renderer     = new MasterRenderer(playerCamera, loader);
    auto guiRenderer  = new GuiRenderer(loader);
    auto rectRenderer = new RectRenderer(loader);

    /**
     * -----------------------------------------------------------------------
     * Fog / Atmosphere (1.6)
     * -----------------------------------------------------------------------
     */
    FogSettings fogSettings;
    fogSettings.mode              = FogMode::ExponentialSquared;
    fogSettings.density           = 0.007f;
    fogSettings.color             = glm::vec3(0.5f, 0.6f, 0.7f);
    fogSettings.heightFogEnabled  = true;
    fogSettings.heightFogStart    = 5.0f;
    fogSettings.heightFogEnd      = -10.0f;
    fogSettings.scatteringEnabled = true;
    fogSettings.sunDirection      = glm::normalize(glm::vec3(0.0f, -1.0f, 0.5f));
    renderer->setFogSettings(fogSettings);

    /**
     * -----------------------------------------------------------------------
     * Shadow Mapping (1.4)
     * -----------------------------------------------------------------------
     */
    renderer->enableShadowMapping(ShadowMap::kDefaultSize);

    /**
     * -----------------------------------------------------------------------
     * Water Rendering (1.5)
     * -----------------------------------------------------------------------
     */
    FrameBuffers* reflectFbo = new FrameBuffers();

    GLuint dudvTex   = 0;
    GLuint waterNorm = 0;
    {
        auto* t = loader->loadTexture("waterDUDV");
        if (t) dudvTex = t->getId();
        t = loader->loadTexture("waterNormal");
        if (t) waterNorm = t->getId();
    }
    // Fallback 1x1 neutral texture when water textures are absent
    auto createFallbackTex = [](GLuint& id, unsigned char r, unsigned char g, unsigned char b) {
        if (id == 0) {
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            unsigned char data[4] = {r, g, b, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    };
    createFallbackTex(dudvTex,   128, 128, 255);  // neutral DuDv
    createFallbackTex(waterNorm, 128, 128, 255);  // flat normal map

    auto* waterShader   = new WaterShader();
    auto* waterRenderer = new WaterRenderer(loader, waterShader, renderer->getProjectionMatrix(),
                                            reflectFbo, dudvTex, waterNorm);
    renderer->setWaterRenderer(waterRenderer);
    // Add water tiles from scene.cfg; fall back to a single hardcoded tile if none configured
    if (waterTiles.empty()) {
        renderer->addWaterTile(WaterTile(0.0f, -1.0f, -200.0f));
    } else {
        for (const auto& tile : waterTiles)
            renderer->addWaterTile(tile);
    }

    /**
     * -----------------------------------------------------------------------
     * PBR Material demo (1.1) — demonstrates metallic-roughness fallback values
     * -----------------------------------------------------------------------
     */
    PBRMaterial pbrMat;
    pbrMat.shader         = renderer->getPBRShader();
    pbrMat.albedoValue    = glm::vec3(0.8f, 0.6f, 0.2f);  // gold-ish
    pbrMat.metallicValue  = 0.9f;
    pbrMat.roughnessValue = 0.2f;
    pbrMat.aoValue        = 1.0f;
    // pbrMat would be applied per-entity; the shader itself is wired and ready.

    /**
     * GUI Creation
     */
    UiMaster::initialize(loader, guiRenderer, fontRenderer, rectRenderer);

    GuiComponent *masterContainer = UiMaster::getMasterComponent();
    masterContainer->initialize();
    UiMaster::applyConstraints();
    UiMaster::createRenderQueue();

    // Show reflection FBO texture as a HUD overlay (debugging aid)
    auto reflectGuiTex = new GuiTexture(reflectFbo->getReflectionTexture(),
                                        glm::vec2(0.75f, 0.75f), glm::vec2(0.2f));
    guis.push_back(reflectGuiTex);

    /**
     * Mouse / Terrain Picker
     */
    Terrain* pickerTerrain = primaryTerrain ? primaryTerrain : allTerrains.front();
    auto picker = new TerrainPicker(playerCamera, renderer->getProjectionMatrix(), pickerTerrain);

    std::vector<Interactive *> allBoxes;
    for (auto e : entities) {
        if (e->getBoundingBox() != nullptr) {
            allBoxes.push_back(e);
        }
    }
    allBoxes.reserve(entities.size());
    allBoxes.insert(allBoxes.end(), entities.begin(), entities.end());

    /**
     * -----------------------------------------------------------------------
     * Phase 2.1 — Skeletal Animation renderer + keyboard-driven transitions
     * -----------------------------------------------------------------------
     * Models are loaded by SceneLoader via animated_character lines in scene.cfg.
     * Here we just create the renderer and wire keyboard conditions to every
     * AnimationController that was set up by SceneLoader.
     */
    AnimatedShader*   animShader   = new AnimatedShader();
    AnimatedRenderer* animRenderer = new AnimatedRenderer(animShader);

    // Keyboard conditions shared by all animated characters
    auto walkCond = []() { return InputMaster::isKeyDown(W) && !InputMaster::isKeyDown(LeftShift); };
    auto runCond  = []() { return InputMaster::isKeyDown(W) &&  InputMaster::isKeyDown(LeftShift); };
    auto jumpCond = []() { return InputMaster::isKeyDown(Space); };

    // Re-wire transitions with live keyboard conditions for every animated character.
    // SceneLoader registered no-op lambdas; replace with real input-driven ones now.
    // setupDefaultTransitions is safe to call even when not all clip states exist —
    // the AnimationController silently ignores transitions to unregistered states.
    for (auto* ae : animatedEntities) {
        if (ae && ae->controller)
            ae->controller->setupDefaultTransitions(walkCond, runCond, jumpCond);
    }

    if (!animatedEntities.empty())
        std::cout << "[MainGuiLoop] " << animatedEntities.size()
                  << " animated character(s) ready for rendering.\n";
    else
        std::cout << "[MainGuiLoop] No animated_character entries in scene.cfg — "
                     "add one to see skeletal animation.\n";

    /**
     * -----------------------------------------------------------------------
     * Phase 2.4 — Instanced Rendering (500+ trees in one draw call)
     * -----------------------------------------------------------------------
     */
    InstancedModel* instancedTreeModel = nullptr;

    {
        ModelData treeData = OBJLoader::loadObjModel("tree");
        if (!treeData.getIndices().empty()) {
            RawModel* rawTree   = loader->loadToVAO(treeData);
            auto*     treeTex   = loader->loadTexture("tree");
            GLuint    treeTexID = treeTex ? treeTex->getId() : 0;

            instancedTreeModel = new InstancedModel(rawTree->getVaoId(),
                                                    rawTree->getVertexCount(),
                                                    treeTexID);
            instancedTreeModel->setupInstanceVBO();

            // Generate 500 random tree transforms on the terrain
            srand(42);
            for (int i = 0; i < 500; ++i) {
                float x = static_cast<float>(rand() % 800) - 400.0f;
                float z = static_cast<float>(rand() % 800) - 400.0f;
                float y = primaryTerrain ? primaryTerrain->getHeightOfTerrain(x, z) : 0.0f;
                float s = 0.5f + static_cast<float>(rand() % 100) / 100.0f;
                instancedTreeModel->addInstance(
                    Maths::createTransformationMatrix(glm::vec3(x, y, z), glm::vec3(0.0f), s));
            }

            std::cout << "[Phase2] Instanced tree model ready — 500 instances.\n";
        } else {
            std::cout << "[Phase2] Could not load tree model for instancing.\n";
        }
    }

    /**
     * -----------------------------------------------------------------------
     * Main Game Loop
     * -----------------------------------------------------------------------
     */
    while (DisplayManager::stayOpen()) {
        // Step the physics simulation first so the player's position is
        // up-to-date before the camera and input systems read it.
        physicsSystem->update(DisplayManager::getFrameTimeSeconds());

        playerCamera->move(pickerTerrain);
        picker->update();

        // Sync every animated character to the player's world position so the
        // character follows the player and the camera always frames it correctly.
        // player->getPosition() is the physics foot position (capsule bottom),
        // which matches the model's natural origin (at the feet), so no offset
        // is needed.
        if (player && !animatedEntities.empty()) {
            for (auto* ae : animatedEntities) {
                if (!ae) continue;
                ae->position = player->getPosition();
                ae->rotation = player->getRotation();
            }
        }

        // --- Shadow pass (1.4) ---
        renderer->renderShadowPass(entities, lights);

        // --- Click picking ---
        if (InputMaster::hasPendingClick()) {
            if (InputMaster::mouseClicked(LeftClick)) {
                renderer->renderBoundingBoxes(allBoxes);
                Color clickColor = Picker::getColor();
                int element = BoundingBoxIndex::getIndexByColor(clickColor);
                Interactive *pClickedModel = InteractiveModel::getInteractiveBox(
                        BoundingBoxIndex::getIndexByColor(clickColor));
                if (pClickedModel != nullptr) {
                    if (auto a = dynamic_cast<Player *>(pClickedModel)) {
                        if (!a->hasMaterial()) {
                            a->setMaterial({500.0f, 500.0f});
                            a->activateMaterial();
                        } else {
                            a->disableMaterial();
                        }
                    }
                    printf("position: x, y, z: (%f, %f, %f)\n",
                           pClickedModel->getPosition().x,
                           pClickedModel->getPosition().y,
                           pClickedModel->getPosition().z);
                }
                const string &renderString =
                        ColorName::toString(clickColor) + ", Element: " + std::to_string(element);
                InputMaster::resetClick();
            }
        }

        // --- Reflection pass for water (1.5) ---
        reflectFbo->bindReflectionFrameBuffer();
        renderer->renderBoundingBoxes(allBoxes);
        reflectFbo->unbindCurrentFrameBuffer();

        // --- Main scene pass via scene graph (1.2) ---
        renderer->renderSceneGraph(sceneGraph, assimpEntities, allTerrains, lights);

        // --- Phase 2.1: Animated characters (loaded via scene.cfg animated_character) ---
        if (!animatedEntities.empty()) {
            animRenderer->render(animatedEntities, DisplayManager::delta, lights,
                                 playerCamera, renderer->getProjectionMatrix());
        }

        // --- Phase 2.4: Instanced rendering (500 trees) ---
        if (instancedTreeModel && instancedTreeModel->getInstanceCount() > 0) {
            renderer->processInstancedEntity(instancedTreeModel,
                                             instancedTreeModel->getInstances());
            renderer->renderInstanced(lights);
        }

        // --- Water rendering after opaque geometry (1.5) ---
        renderer->renderWater(playerCamera, lights.empty() ? nullptr : lights[0]);

        // --- Phase 3: Physics debug wireframes (toggled with F3) ---
        if (playerCamera)
            physicsSystem->renderDebug(playerCamera->getViewMatrix(),
                                       renderer->getProjectionMatrix());

        // --- Render scene.cfg GUI texture overlays, then UiMaster components ---
        guiRenderer->render(guis);
        UiMaster::render();
        // --- Render scene.cfg text overlays registered in TextMaster ---
        TextMaster::render();

        DisplayManager::updateDisplay();
    }

    /**
     * Clean up
     */
    reflectFbo->cleanUp();
    TextMaster::cleanUp();
    fontRenderer->cleanUp();
    guiRenderer->cleanUp();
    rectRenderer->cleanUp();
    renderer->cleanUp();
    loader->cleanUp();
    waterShader->cleanUp();
    // Phase 2 cleanup
    animShader->cleanUp();
    delete animRenderer;
    for (auto* ae : animatedEntities) {
        if (ae) {
            if (ae->model) { ae->model->cleanUp(); delete ae->model; }
            delete ae->controller;
            delete ae;
        }
    }
    delete instancedTreeModel;

    // Phase 3: physics shutdown
    physicsSystem->shutdown();
    delete physicsSystem;

    DisplayManager::closeDisplay();
}

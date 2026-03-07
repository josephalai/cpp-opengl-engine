//
// Created as part of Phase 0 engine refactoring.
// Splits MainGameLoop::main() into a clean Engine class with lifecycle methods.
// The game loop delegates all per-frame work to ordered ISystem instances.
//

#include "Engine.h"
#include "SceneLoader.h"
#include "SceneLoaderJson.h"
#include "InputSystem.h"
#include "InputDispatcher.h"
#include "AnimationSystem.h"
#include "../Events/EventBus.h"
#include "RenderSystem.h"
#include "UISystem.h"
#include "GLUploadQueue.h"
#include "StreamingSystem.h"
#include "../Streaming/ChunkManager.h"
#include "../Physics/PhysicsSystem.h"
#include "../Util/FileSystem.h"
#include "../Util/Utils.h"
#include "../Util/LightUtil.h"
#include "../Util/ColorName.h"
#include "../Util/CommonHeader.h"
#include "../RenderEngine/DisplayManager.h"
#include "../RenderEngine/ObjLoader.h"
#include "../RenderEngine/InstancedModel.h"
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"
#include "../Guis/Text/GUIText.h"
#include "../Guis/GuiComponent.h"
#include "../Guis/Constraints/UiConstraints.h"
#include "../Textures/TerrainTexture.h"
#include "../Textures/TerrainTexturePack.h"
#include "../BoundingBox/BoundingBoxIndex.h"
#include "../Toolbox/Picker.h"
#include "../Input/InputMaster.h"
#include "../Atmosphere/FogSettings.h"
#include "../Shadows/ShadowMap.h"
#include "NetworkSystem.h"
#include "../Network/NetworkPackets.h"
#include "../Entities/Components/NetworkSyncComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/EntityOwnerComponent.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/LightComponent.h"
#include "../ECS/Components/TerrainComponent.h"
#include "../ECS/Components/ActiveChunkTag.h"
#include "../Streaming/ChunkManager.h"
#include "../Toolbox/Maths.h"
#include <enet/enet.h>
#include <fstream>
#include <algorithm>

// ---------------------------------------------------------------------------

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::init() {
    // Initialize ENet before anything else that might use networking.
    if (enet_initialize() != 0) {
        std::cerr << "[Engine] Failed to initialize ENet.\n";
    }

    // Read server IP from ip.cfg (or default to 127.0.0.1).
    loadIPConfig();

    DisplayManager::createDisplay();
    loader = new Loader();

    initFonts();
    loadScene();
    initRenderers();
    initGui();
    initFramebuffersAndPickers();
    buildSystems();

    // --- ECS Phase 2 Step 2 verification ---
    {
        auto view = registry.view<TransformComponent>();
        int count = 0;
        for (auto e : view) { (void)e; ++count; }
        std::cout << "[ECS] Phase 2 Step 2 complete: " << count
                  << " entities with TransformComponent in registry.\n";
    }
}

void Engine::initFonts() {
    TextMaster::init(loader);

    fontRenderer = new FontRenderer();
    arialFont  = new FontType(TextMeshData::loadFont("arial",  48));
    noodleFont = new FontType(TextMeshData::loadFont("noodle", 48));

    fontModel = loader->loadFontVAO();

    auto text1 = new GUIText(
        "This is sample text because I know what I am doing whether you like it or not so I am joe."
        "This is sample text because I know what I am doing whether you like it or not so I am joe.",
        0.50f, fontModel, noodleFont, glm::vec2(25.0f, 225.0f), ColorName::Whitesmoke,
        0.50f * static_cast<float>(DisplayManager::Width()), false);
    texts.push_back(text1);

    pNameText = new GUIText("Joseph Alai MCMXII", 0.5f, fontModel, arialFont,
                            glm::vec2(540.0f, 50.0f), ColorName::Cyan,
                            0.75f * static_cast<float>(DisplayManager::Width()), false);
    texts.push_back(pNameText);

    clickColorText = new GUIText("Color: ", 0.5f, fontModel, arialFont,
                                 glm::vec2(10.0f, 20.0f), ColorName::Green,
                                 0.75f * static_cast<float>(DisplayManager::Width()), false);
    texts.push_back(clickColorText);
}

void Engine::loadScene() {
    // Load 3-D scene content from the config.
    // Prefers scene.json (JSON format, Phase 1 Step 3); falls back to scene.cfg
    // (legacy text format) when scene.json is absent or cannot be parsed.
    const std::string jsonPath = FileSystem::Scene("scene.json");
    const std::string cfgPath  = FileSystem::Scene("scene.cfg");

    // -----------------------------------------------------------------------
    // Temporary accumulators — used only during loading.  After loading,
    // their contents are migrated into the EnTT registry as pure components so
    // that Systems can discover them via registry.view<> instead of side-vectors.
    // Nothing lives outside the registry after this function returns.
    // -----------------------------------------------------------------------
    std::vector<Entity*>         tmpEntities;
    std::vector<AssimpEntity*>   tmpScenes;
    std::vector<Light*>          tmpLights;
    std::vector<Terrain*>        tmpTerrains;
    std::vector<AnimatedEntity*> tmpAnimated;

    std::vector<SceneLoader::PhysicsBodyCfg>   physicsBodyCfgs;
    std::vector<SceneLoader::PhysicsGroundCfg> physicsGroundCfgs;

    bool sceneLoaded = false;
    {
        std::ifstream probe(jsonPath);
        if (probe.is_open()) {
            probe.close();
            sceneLoaded = SceneLoaderJson::load(jsonPath, loader,
                              registry,
                              tmpEntities, tmpScenes, tmpLights,
                              tmpTerrains, guis, texts, waterTiles,
                              primaryTerrain, player, playerCamera,
                              tmpAnimated,
                              physicsBodyCfgs, physicsGroundCfgs);
            if (!sceneLoaded) {
                std::cerr << "[Engine] scene.json load failed — falling back to scene.cfg\n";
            }
        }
    }
    if (!sceneLoaded) {
        sceneLoaded = SceneLoader::load(cfgPath, loader,
                          registry,
                          tmpEntities, tmpScenes, tmpLights,
                          tmpTerrains, guis, texts, waterTiles,
                          primaryTerrain, player, playerCamera,
                          tmpAnimated,
                          physicsBodyCfgs, physicsGroundCfgs);
    }

    // -----------------------------------------------------------------------
    // Migrate temporaries → registry components
    // -----------------------------------------------------------------------

    // Entity* → EntityOwnerComponent so Systems can discover them via view
    for (auto* e : tmpEntities) {
        registry.emplace<EntityOwnerComponent>(e->getHandle(), e);
    }

    // AssimpEntity* → AssimpModelComponent (holds AssimpMesh* + legacy entity*)
    for (auto* s : tmpScenes) {
        auto h = registry.create();
        registry.emplace<AssimpModelComponent>(h, s->getModel(), s);
    }

    // Light* → LightComponent (stored by value — Light data copied inline)
    for (auto* l : tmpLights) {
        auto h = registry.create();
        auto& lc = registry.emplace<LightComponent>(h);
        lc.light = *l;  // copy Light data into the component by value
        delete l;       // original heap object no longer needed
    }

    // Terrain* → TerrainComponent
    for (auto* t : tmpTerrains) {
        auto h = registry.create();
        registry.emplace<TerrainComponent>(h, t);
    }

    // AnimatedEntity* → AnimatedEntity component (copy fields into registry)
    for (auto* ae : tmpAnimated) {
        ae->isLocalPlayer = true;
        ae->ownsModel     = true;
        auto h = registry.create();
        registry.emplace<AnimatedEntity>(h, *ae); // copy by value into registry
        delete ae; // fields copied; original wrapper no longer needed
    }

    // -----------------------------------------------------------------------
    // Physics world setup
    // -----------------------------------------------------------------------
    physicsSystem = new PhysicsSystem();
    physicsSystem->init();

    for (const auto& g : physicsGroundCfgs) {
        physicsSystem->addGroundPlane(g.yHeight);
    }

    // physicsBodyCfgs uses entityIndex into tmpEntities (same ordering as before)
    for (const auto& cfg : physicsBodyCfgs) {
        if (cfg.entityIndex < 0 || cfg.entityIndex >= static_cast<int>(tmpEntities.size()))
            continue;
        Entity* ent = tmpEntities[static_cast<size_t>(cfg.entityIndex)];

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

    // -----------------------------------------------------------------------
    // Fallback defaults when scene files are missing / incomplete
    // -----------------------------------------------------------------------
    if (!sceneLoaded || !player || !playerCamera) {
        std::cerr << "[Engine] SceneLoader failed or missing player — using minimal defaults\n";

        if (registry.view<TerrainComponent>().begin() == registry.view<TerrainComponent>().end()) {
            auto* bgTex  = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/grass")->getId());
            auto* rTex   = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/dirt")->getId());
            auto* gTex   = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blueflowers")->getId());
            auto* bTex   = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/brickroad")->getId());
            auto* pack   = new TerrainTexturePack(bgTex, rTex, gTex, bTex);
            auto* blend  = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blendMap")->getId());
            primaryTerrain = new Terrain(0, -1, loader, pack, blend, terrainHeightmapFile);
            auto h = registry.create();
            registry.emplace<TerrainComponent>(h, primaryTerrain);
        }

        if (registry.view<LightComponent>().begin() == registry.view<LightComponent>().end()) {
            auto h = registry.create();
            auto& lc = registry.emplace<LightComponent>(h);
            lc.light = Light(glm::vec3(0.0f, 1000.0f, -7000.0f),
                             glm::vec3(0.4f, 0.4f, 0.4f), {
                .ambient  = glm::vec3(0.2f, 0.2f, 0.2f),
                .diffuse  = glm::vec3(0.5f, 0.5f, 0.5f),
                .constant = Light::kDirectional,
            });
        }

        if (!player) {
            ModelData stallData = OBJLoader::loadObjModel("Stall");
            if (!stallData.getIndices().empty()) {
                BoundingBoxData bbData = OBJLoader::loadBoundingBox(
                    stallData, ClickBoxTypes::BOX, BoundTypes::AABB);
                auto* pBox  = loader->loadToVAO(bbData);
                auto* model = new TexturedModel(loader->loadToVAO(stallData),
                    new ModelTexture("stallTexture", PNG, Material{2.0f, 2.0f}));
                player = new Player(registry, model,
                    new BoundingBox(pBox, BoundingBoxIndex::genUniqueId()),
                    glm::vec3(100.0f, 3.0f, -50.0f),
                    glm::vec3(0.0f, 180.0f, 0.0f), 1.0f);
                InteractiveModel::setInteractiveBox(player);
                registry.emplace<EntityOwnerComponent>(player->getHandle(),
                                                       static_cast<Entity*>(player));
            } else {
                std::cerr << "[Engine] Could not load Stall fallback model\n";
            }
        }

        if (!playerCamera && player) {
            playerCamera = new PlayerCamera(player);
        }
    }

    // Register heightfield colliders for all terrain tiles (from registry).
    {
        auto tView = registry.view<TerrainComponent>();
        for (auto [e, tc] : tView.each()) {
            if (tc.terrain) physicsSystem->addTerrainCollider(tc.terrain);
        }
    }

    // Set up kinematic character controller for the player.
    if (player) {
        physicsSystem->setCharacterController(player, 1.0f, 3.0f);
        player->setPhysicsSystem(physicsSystem);
    }
}

void Engine::initRenderers() {
    renderer    = new MasterRenderer(playerCamera, loader);
    guiRenderer = new GuiRenderer(loader);
    rectRenderer = new RectRenderer(loader);

    // Animation renderer
    animShader   = new AnimatedShader();
    animRenderer = new AnimatedRenderer(animShader);

    // Fog / Atmosphere
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

    // Shadow mapping
    renderer->enableShadowMapping(ShadowMap::kDefaultSize);

    UiMaster::initialize(loader, guiRenderer, fontRenderer, rectRenderer);
}

void Engine::initGui() {
    // sampleModifiedGui is the extra lifebar that slides across the screen;
    // its texture comes from the first GUI texture loaded by SceneLoader
    // (gui/lifebar), which is already in the guis vector.
    // We create a separate copy at a different position for the animation.
    sampleModifiedGui = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(),
                                       glm::vec2(-0.72f, 0.3f),
                                       glm::vec2(0.290f, 0.0900f) / 3.0f);
    guis.push_back(sampleModifiedGui);

    Color     color   = ColorName::Cyan;
    glm::vec2 position  = glm::vec2(-0.75f, 0.67f);
    glm::vec2 size      = glm::vec2(0.290f, 0.0900f);
    glm::vec2 scale     = glm::vec2(0.25f,  0.33f);
    float     alpha     = 0.33f;

    auto guiRect  = new GuiRect(color, position, size, scale, alpha);
    glm::vec2 position2 = glm::vec2(-0.55f, 0.37f);
    Color     color2    = ColorName::Green;
    auto guiRect2 = new GuiRect(color2, position2, size, scale, alpha);
    rects.push_back(guiRect);

    sampleModifiedGui->addChild(sampleModifiedGui, new UiConstraints(0.0f, 0.0f, 200, 200));

    masterContainer = UiMaster::getMasterComponent();
    auto parent     = new GuiComponent(Container::CONTAINER, new UiConstraints(0.01f, -0.01f, 50, 50));
    parent->setName("Parent");
    masterContainer->setName("Master Container");

    // Retrieve the GUI textures added by SceneLoader in config order:
    //   guis[0] = gui/lifebar, guis[1] = gui/green, guis[2] = gui/heart
    GuiTexture* t1 = guis.size() > 0 ? guis[0] : nullptr;
    GuiTexture* t2 = guis.size() > 1 ? guis[1] : nullptr;
    GuiTexture* t3 = guis.size() > 2 ? guis[2] : nullptr;

    if (t1) { t1->setName("gui/lifebar"); }
    if (t2) { t2->setName("gui/green"); }
    if (t3) { t3->setName("gui/heart"); }
    guiRect->setName("GuiRect");
    guiRect2->setName("GuiRect2");

    masterContainer->addChild(guiRect,   new UiConstraints(0.0f,  -0.1f, 50, 50));
    masterContainer->addChild(guiRect2,  new UiConstraints(0.0f,  -0.1f, 50, 50));
    masterContainer->addChild(parent,    new UiConstraints(0.02f, -0.1f, 50, 50));
    if (t1) parent->addChild(t1, new UiConstraints(0.00f, -0.1f, 50, 50));
    if (t2) parent->addChild(t2, new UiConstraints(0.00f, -0.1f, 50, 50));
    if (t3) parent->addChild(t3, new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(texts[0],    new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(texts[1],    new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(clickColorText, new UiConstraints(0.00f, -0.1f, 50, 50));
    if (t1) t1->addChild(pNameText, new UiConstraints(-500.00f, 40.1f, 50, 50));

    masterContainer->initialize();
    UiMaster::createRenderQueue(masterContainer);
    UiMaster::applyConstraints(masterContainer);
}

void Engine::initFramebuffersAndPickers() {
    reflectFbo = new FrameBuffers();
    auto gui   = new GuiTexture(reflectFbo->getReflectionTexture(), glm::vec2(0.75f, 0.75f), glm::vec2(0.2f));
    guis.push_back(gui);

    // Water renderer — loads optional DuDv / normal textures, falls back to neutral 1×1 textures.
    // Always created so water tiles (from scene.cfg or the default) are rendered.
    {
        GLuint dudvTex   = 0;
        GLuint waterNorm = 0;
        auto* t = loader->loadTexture("waterDUDV");
        if (t) dudvTex = t->getId();
        t = loader->loadTexture("waterNormal");
        if (t) waterNorm = t->getId();

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
        createFallbackTex(dudvTex,   128, 128, 255);
        createFallbackTex(waterNorm, 128, 128, 255);

        waterShader   = new WaterShader();
        waterRenderer = new WaterRenderer(loader, waterShader, renderer->getProjectionMatrix(),
                                          reflectFbo, dudvTex, waterNorm);
        renderer->setWaterRenderer(waterRenderer);

        // Add water tiles from scene.cfg; fall back to a single default tile if none configured
        if (waterTiles.empty()) {
            renderer->addWaterTile(WaterTile(0.0f, -1.0f, -200.0f));
        } else {
            for (const auto& tile : waterTiles)
                renderer->addWaterTile(tile);
        }
    }

    picker = new TerrainPicker(playerCamera, renderer->getProjectionMatrix(), primaryTerrain);

    // Wire keyboard-driven animation transitions for every animated character.
    // setupDefaultTransitions is safe to call when not all clip states exist —
    // the controller silently ignores transitions to unregistered states.
    {
        auto animView = registry.view<AnimatedEntity>();
        for (auto [e, ae] : animView.each()) {
            if (ae.controller && ae.isLocalPlayer) {
                ae.controller->setupDefaultTransitions(
                    []() { return InputMaster::isKeyDown(W) && !InputMaster::isKeyDown(LeftShift); },
                    []() { return InputMaster::isKeyDown(W) &&  InputMaster::isKeyDown(LeftShift); },
                    []() { return InputMaster::isKeyDown(Space); });
            }
        }
    }

    {
        auto animCount = registry.view<AnimatedEntity>().size_hint();
        if (animCount > 0)
            std::cout << "[Engine] " << animCount << " animated character(s) ready.\n";
        else
            std::cout << "[Engine] No animated_character entries in scene.cfg.\n";
    }

    // Phase 2.4 — Instanced Rendering (500+ trees in one draw call)
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

            srand(42);
            for (int i = 0; i < 500; ++i) {
                float x = static_cast<float>(rand() % 800) - 400.0f;
                float z = static_cast<float>(rand() % 800) - 400.0f;
                float y = primaryTerrain ? primaryTerrain->getHeightOfTerrain(x, z) : 0.0f;
                float s = 0.5f + static_cast<float>(rand() % 100) / 100.0f;
                instancedTreeModel->addInstance(
                    Maths::createTransformationMatrix(glm::vec3(x, y, z), glm::vec3(0.0f), s));
            }
            std::cout << "[Engine] Instanced tree model ready — 500 instances.\n";
        } else {
            std::cout << "[Engine] Could not load tree model for instancing.\n";
        }
    }
}

void Engine::loadIPConfig() {
    std::ifstream cfgFile("ip.cfg");
    if (cfgFile.is_open()) {
        std::string ip;
        if (std::getline(cfgFile, ip)) {
            // Trim whitespace.
            auto start = ip.find_first_not_of(" \t\r\n");
            auto end   = ip.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                serverIP_ = ip.substr(start, end - start + 1);
            }
        }
        cfgFile.close();
    }
    std::cout << "[Engine] Server IP: " << serverIP_ << "\n";
}

Entity* Engine::onNetworkSpawn(uint32_t networkId,
                               const std::string& modelType,
                               const glm::vec3& position) {
    // Reuse the local player's Assimp-loaded TexturedModel for remote players.
    // OBJLoader cannot load .glb/.gltf — the player character comes from
    // SceneLoader/Assimp, so we share the same model pointer.
    TexturedModel* remoteModel = nullptr;
    glm::vec3 modelMin;
    glm::vec3 modelMax;
    if (player && player->getModel()) {
        remoteModel = player->getModel();
        // Use character-sized default bounds when reusing the player model.
        modelMin = glm::vec3(-0.5f, 0.0f, -0.5f);
        modelMax = glm::vec3( 0.5f, 2.0f,  0.5f);
    } else {
        // Last-resort OBJ fallback
        ModelData fallbackData = OBJLoader::loadObjModel("Stall");
        if (fallbackData.getIndices().empty()) {
            std::cerr << "[Engine] onNetworkSpawn FAILED — no model for remote entity "
                      << networkId << "\n";
            return nullptr;
        }
        auto* tex = new ModelTexture("stallTexture", PNG, Material{2.0f, 2.0f});
        remoteModel = new TexturedModel(loader->loadToVAO(fallbackData), tex);
        modelMin = fallbackData.getMin();
        modelMax = fallbackData.getMax();
    }

    // Each entity needs its own BoundingBox (unique picking colour).
    AABB bbRegion(BoundTypes::AABB);
    bbRegion.min = modelMin;
    bbRegion.max = modelMax;
    std::vector<float> boxVerts = OBJLoader::generateBox(modelMin, modelMax);
    BoundingBoxData bbData(bbRegion, std::move(boxVerts));
    auto* rawBB = loader->loadToVAO(bbData);
    auto* bb    = new BoundingBox(rawBB, BoundingBoxIndex::genUniqueId());
    bb->setAABB(modelMin, modelMax);

    // Match the local player's scale so the physics-proxy model (e.g. the
    // invisible stall at scale=0 in scene.cfg) stays hidden on remote clients.
    // The actual visual is provided by the AnimatedEntity created below.
    const float remoteScale = player ? player->getScale() : 1.0f;
    auto* ent = new Entity(registry, remoteModel, bb, position, glm::vec3(0.0f), remoteScale);

    // Attach the interpolation component so the entity can receive and
    // smoothly interpolate server transform snapshots.
    ent->addComponent<NetworkSyncComponent>();

    // Register in registry so RenderSystem can discover this entity via view.
    registry.emplace<EntityOwnerComponent>(ent->getHandle(), ent);

    // Register with ChunkManager so the StreamingSystem includes this entity
    // in the active render list.
    if (chunkManager) {
        chunkManager->registerEntity(ent, position);
    }

    // Create an AnimatedEntity component so the remote player renders with the
    // animated character mesh (same AnimatedModel as local player — safe to
    // share since AnimatedRenderer renders entities sequentially).
    {
        // Find the local player's animated model from the registry.
        AnimatedModel* sharedModel = nullptr;
        float          firstScale  = 1.0f;
        auto animView = registry.view<AnimatedEntity>();
        for (auto [ae_handle, ae] : animView.each()) {
            if (ae.isLocalPlayer && ae.model) {
                sharedModel = ae.model;
                firstScale  = ae.scale;
                break;
            }
        }

        if (sharedModel) {
            // Normalize animation clip names identically to SceneLoader.
            auto normalizeClipName = [](const std::string& raw) -> std::string {
                std::string lower(raw);
                for (auto& c : lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (lower.find("idle") != std::string::npos) return "Idle";
                if (lower.find("walk") != std::string::npos) return "Walk";
                if (lower.find("run")  != std::string::npos) return "Run";
                if (lower.find("jump") != std::string::npos) return "Jump";
                std::string out = raw;
                if (!out.empty())
                    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
                return out;
            };

            auto* remoteCtrl = new AnimationController();
            for (auto& clip : sharedModel->clips) {
                remoteCtrl->addState(normalizeClipName(clip.name), &clip);
            }
            remoteCtrl->setState("Idle");

            AnimatedEntity remoteAe;
            remoteAe.model        = sharedModel;
            remoteAe.controller   = remoteCtrl;
            remoteAe.position     = position;
            remoteAe.scale        = firstScale;
            remoteAe.modelOffset  = glm::vec3(0.0f);
            remoteAe.isLocalPlayer = false;
            remoteAe.ownsModel    = false;
            remoteAe.pairedEntity = ent;

            auto aeHandle = registry.create();
            registry.emplace<AnimatedEntity>(aeHandle, remoteAe);
        }
    }

    std::cout << "[Engine] Remote entity " << networkId
              << " (model=\"" << modelType << "\") spawned at ("
              << position.x << ", " << position.y << ", " << position.z << ")\n";

    return ent;
}

void Engine::onNetworkDespawn(uint32_t /*networkId*/, Entity* e) {
    // Remove the paired AnimatedEntity component from the registry so
    // AnimationSystem no longer renders or accesses the dangling pairedEntity ptr.
    {
        auto animView = registry.view<AnimatedEntity>();
        for (auto [ae_handle, ae] : animView.each()) {
            if (ae.pairedEntity == e) {
                delete ae.controller; // controller is owned per remote entity
                registry.destroy(ae_handle);
                break;
            }
        }
    }

    // Deregister from ChunkManager so StreamingSystem no longer
    // returns this entity in the active list.
    if (chunkManager) {
        chunkManager->removeEntity(e);
    }

    // Deleting the Entity* calls Entity::~Entity() which destroys the entt
    // entity handle and all its registry components (EntityOwnerComponent,
    // TransformComponent, etc.).
    delete e;
}

void Engine::buildSystems() {
    // Systems are updated in this order each frame:
    //   1. InputDispatcher — translate raw InputMaster state into EventBus events
    //   2. PhysicsSystem   — step simulation, sync transforms
    //   3. InputSystem     — camera movement, picker, GUI animations
    //   4. StreamingSystem — update chunk loading, refresh entity/terrain lists
    //   5. NetworkSystem   — ENet packet polling + NetworkSyncComponent interpolation
    //   6. RenderSystem    — FBO + main scene render (frustum-culled)
    //   7. AnimationSystem — sync positions + animated character render
    //   8. UISystem        — object picking + UiMaster render + constraints

    // InputDispatcher must run first so that PlayerMoveCommandEvent subscribers
    // (e.g. Player) have up-to-date speed values before any other system runs.
    systems.push_back(std::make_unique<InputDispatcher>(picker));

    // Subscribe the player to receive movement commands via the EventBus
    // instead of polling InputMaster::isKeyDown() directly in checkInputs().
    if (player) {
        player->subscribeToEvents();
    }

    if (physicsSystem) {
        systems.push_back(std::unique_ptr<ISystem>(physicsSystem));
    }

    systems.push_back(std::make_unique<InputSystem>(
        playerCamera, primaryTerrain, picker, sampleModifiedGui, pNameText,
        player, physicsSystem));

    // Build the chunk manager from the first loaded terrain's texture config.
    // Initial scene entities are registered so they appear inside their chunk.

    if (primaryTerrain) {
        chunkManager = new ChunkManager(loader,
                                        primaryTerrain->getTexturePack(),
                                        primaryTerrain->getBlendMap(),
                                        terrainHeightmapFile);
        // Register terrain tiles so ChunkManager tracks them.
        {
            auto tView = registry.view<TerrainComponent>();
            for (auto [h, tc] : tView.each()) {
                if (tc.terrain) chunkManager->registerTerrain(tc.terrain);
            }
        }
        // Register entities from the registry into the chunk grid.
        {
            auto eView = registry.view<EntityOwnerComponent>();
            for (auto [h, eoc] : eView.each()) {
                if (eoc.ptr) chunkManager->registerEntity(eoc.ptr, eoc.ptr->getPosition());
            }
        }
        {
            auto sView = registry.view<AssimpModelComponent>();
            for (auto [h, am] : sView.each()) {
                if (am.entity) chunkManager->registerAssimpEntity(am.entity, am.entity->getPosition());
            }
        }
        systems.push_back(std::make_unique<StreamingSystem>(
            chunkManager, player, registry));
    } else {
        // No streaming — tag all entities/terrains/scenes as permanently active.
        {
            auto v = registry.view<EntityOwnerComponent>();
            for (auto e : v) registry.emplace_or_replace<ActiveChunkTag>(e);
        }
        {
            auto v = registry.view<AssimpModelComponent>();
            for (auto e : v) registry.emplace_or_replace<ActiveChunkTag>(e);
        }
        {
            auto v = registry.view<TerrainComponent>();
            for (auto e : v) registry.emplace_or_replace<ActiveChunkTag>(e);
        }
    }

    // NetworkSystem connects to the headless server via ENet and interpolates
    // all network entities.  The local Player entity IS the network entity —
    // no separate dummy entity is needed.
    {
        auto netSys = std::make_unique<NetworkSystem>(
            serverIP_,
            player,
            // Spawn callback
            [this](uint32_t nid, const std::string& model,
                   const glm::vec3& pos) -> Entity* {
                return onNetworkSpawn(nid, model, pos);
            },
            // Despawn callback
            [this](uint32_t nid, Entity* e) {
                onNetworkDespawn(nid, e);
            }
        );

        netSys->init();

        networkSystem_ = netSys.get();
        systems.push_back(std::move(netSys));
    }

    systems.push_back(std::make_unique<RenderSystem>(
        renderer, reflectFbo, registry,
        playerCamera, renderer->getProjectionMatrix(), instancedTreeModel));

    systems.push_back(std::make_unique<AnimationSystem>(
        animRenderer, registry, player,
        playerCamera, renderer->getProjectionMatrix()));

    systems.push_back(std::make_unique<UISystem>(
        renderer, registry, clickColorText, masterContainer,
        guiRenderer, guis));
}

void Engine::run() {
    while (DisplayManager::stayOpen()) {
        float dt = DisplayManager::getFrameTimeSeconds();
        for (auto& sys : systems) {
            sys->update(dt);
        }
        // Render physics debug lines after all render systems have drawn
        if (physicsSystem && playerCamera) {
            physicsSystem->renderDebug(
                playerCamera->getViewMatrix(),
                renderer->getProjectionMatrix());
        }
        DisplayManager::updateDisplay();
        // Process any pending GL upload tasks (from async resource loading).
        GLUploadQueue::instance().processAll(/*maxPerFrame=*/10);
    }
}

void Engine::shutdown() {
    systems.clear();  // ISystem destructors release per-system resources
                      // (PhysicsSystem is the first entry; it cleans up Bullet)

    // Clear all EventBus subscriptions before entity/player are destroyed.
    // This prevents any late event delivery from reaching freed objects.
    EventBus::instance().clear();

    delete instancedTreeModel;
    instancedTreeModel = nullptr;

    reflectFbo->cleanUp();
    TextMaster::cleanUp();
    fontRenderer->cleanUp();
    guiRenderer->cleanUp();
    rectRenderer->cleanUp();
    renderer->cleanUp();   // also cleans up waterRenderer if set
    if (waterShader) waterShader->cleanUp();
    if (animShader)  {
        animShader->getBoneBuffer().cleanup();
        animShader->cleanUp();
    }

    // Clean up animated entities from the registry.
    // Only delete the model when the component owns it (local-player entities);
    // remote entities share the pointer and must NOT delete it.
    {
        auto view = registry.view<AnimatedEntity>();
        for (auto [e, ae] : view.each()) {
            if (ae.ownsModel && ae.model) {
                ae.model->cleanUp();
                delete ae.model;
            }
            delete ae.controller;
        }
    }

    // Lights are stored by value in LightComponent — no manual delete needed.

    delete animRenderer;
    loader->cleanUp();
    DisplayManager::closeDisplay();

    // Tear down ENet after all systems (including NetworkSystem) are destroyed.
    enet_deinitialize();
}

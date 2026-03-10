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
#include "../RenderEngine/InstancedModelManager.h"
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
#include "PlayerMovementSystem.h"
#include "NetworkInterpolationSystem.h"
#include "OriginShiftSystem.h"
#include "../Network/NetworkPackets.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../ECS/Components/NetworkSyncData.h"
#include "../Toolbox/Maths.h"
#include "../Config/ConfigManager.h"
#include "../Config/PrefabManager.h"
#include "../Config/EntityFactory.h"
#include <enet/enet.h>
#include <thread>
#include <fstream>
#include <algorithm>

// ---------------------------------------------------------------------------

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::init() {
    // --- Data-driven initialisation (must happen before DisplayManager) ---
    ConfigManager::get().loadAll(HOME_PATH);
    PrefabManager::get().loadAll(HOME_PATH);

    // Load input action bindings from controls.json.
    InputMaster::loadBindings(std::string(HOME_PATH) + "/src/Resources/controls.json");

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

    // --- ECS Phase 2 Step 3 verification ---
    {
        auto viewT = registry.view<TransformComponent>();
        auto viewI = registry.view<InputStateComponent>();
        auto viewN = registry.view<NetworkSyncData>();
        int countT = 0, countI = 0, countN = 0;
        for (auto e : viewT) { (void)e; ++countT; }
        for (auto e : viewI) { (void)e; ++countI; }
        for (auto e : viewN) { (void)e; ++countN; }
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

    std::vector<SceneLoader::PhysicsBodyCfg>   physicsBodyCfgs;
    std::vector<SceneLoader::PhysicsGroundCfg> physicsGroundCfgs;

    bool sceneLoaded = false;
    {
        std::ifstream probe(jsonPath);
        if (probe.is_open()) {
            probe.close();
            sceneLoaded = SceneLoaderJson::load(jsonPath, loader,
                              registry,
                              entities, lights,
                              allTerrains, guis, texts, waterTiles,
                              primaryTerrain, player, playerCamera,
                              animatedEntities,
                              physicsBodyCfgs, physicsGroundCfgs);
            if (!sceneLoaded) {
                std::cerr << "[Engine] scene.json load failed — falling back to scene.cfg\n";
            }
        }
    }
    if (!sceneLoaded) {
        sceneLoaded = SceneLoader::load(cfgPath, loader,
                          registry,
                          entities, lights,
                          allTerrains, guis, texts, waterTiles,
                          primaryTerrain, player, playerCamera,
                          animatedEntities,
                          physicsBodyCfgs, physicsGroundCfgs);
    }

    // All animated entities loaded by SceneLoader belong to the local player's
    // character(s).  Mark them so AnimationSystem doesn't treat them as remote.
    // Also mark them as model owners so Engine::shutdown() knows to delete the
    // model for these entities (remote entities share the pointer and must not).
    for (auto* ae : animatedEntities) {
        if (ae) {
            ae->isLocalPlayer = true;
            ae->ownsModel     = true;
        }
    }

    // Set up physics world and register bodies from config
    physicsSystem = new PhysicsSystem();
    physicsSystem->setRegistry(registry);  // Bind ECS registry for TransformComponent sync
    physicsSystem->init();

    // Add ground planes
    for (const auto& g : physicsGroundCfgs) {
        physicsSystem->addGroundPlane(g.yHeight);
    }

    // Add rigid bodies for named entities
    for (const auto& cfg : physicsBodyCfgs) {
        // entityIndex was resolved by SceneLoader against the entities vector
        if (cfg.entityIndex < 0 || cfg.entityIndex >= static_cast<int>(entities.size()))
            continue;
        Entity* ent = entities[static_cast<size_t>(cfg.entityIndex)];

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

    // Fallback: if neither scene.json nor scene.cfg provided a player or camera,
    // build minimal defaults so the engine always has something to render.
    if (!sceneLoaded || !player || !playerCamera) {
        std::cerr << "[Engine] SceneLoader failed or missing player — using minimal defaults\n";

        if (allTerrains.empty()) {
            auto* bgTex  = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/grass")->getId());
            auto* rTex   = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/dirt")->getId());
            auto* gTex   = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blueflowers")->getId());
            auto* bTex   = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/brickroad")->getId());
            auto* pack   = new TerrainTexturePack(bgTex, rTex, gTex, bTex);
            auto* blend  = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blendMap")->getId());
            primaryTerrain = new Terrain(0, -1, loader, pack, blend, terrainHeightmapFile);
            allTerrains.push_back(primaryTerrain);
        }

        if (lights.empty()) {
            lights.push_back(new Light(glm::vec3(0.0f, 1000.0f, -7000.0f),
                                       glm::vec3(0.4f, 0.4f, 0.4f), {
                .ambient  = glm::vec3(0.2f, 0.2f, 0.2f),
                .diffuse  = glm::vec3(0.5f, 0.5f, 0.5f),
                .constant = Light::kDirectional,
            }));
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
                entities.push_back(player);
            } else {
                std::cerr << "[Engine] Could not load Stall fallback model\n";
            }
        }

        if (!playerCamera && player) {
            playerCamera = new PlayerCamera(player);
        }
    }

    // Register heightfield colliders for all terrain tiles so Bullet knows
    // about the ground geometry for character/rigid-body collision.
    for (auto* t : allTerrains) {
        physicsSystem->addTerrainCollider(t);
    }

    // [Phase 3.3] The server-authoritative architecture drives the local player's
    // position with SharedMovement::applyInput() (direct Euler math), NOT Bullet.
    // Registering a btKinematicCharacterController would cause PhysicsSystem to
    // overwrite the player's TransformComponent with Bullet's output every tick,
    // producing a completely different position than the server computes → constant
    // reconciliation snapping.  The legacy path in PlayerMovementSystem uses the
    // same Euler integration as SharedMovement, so prediction and server agree.
    //
    physicsSystem->setCharacterController(player, 0.5f, 1.8f); // [Phase 3.3] Removed
    player->setPhysicsSystem(physicsSystem);                    // [Phase 3.3] Removed (no-op)

    // Emplace the new ECS InputStateComponent so PlayerMovementSystem can drive it.
    if (player) {
        auto& isc     = registry.emplace_or_replace<InputStateComponent>(player->getHandle());
        isc.terrain       = primaryTerrain;
        // [Phase 3.3] Leave physicsSystem null so PlayerMovementSystem uses the
        // legacy direct-math path instead of the Bullet physics path.  This keeps
        // client movement mathematically identical to SharedMovement::applyInput().
        isc.physicsSystem = physicsSystem;
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
    // SceneLoader registered no-op lambdas; replace with real input-driven ones.
    // setupDefaultTransitions is safe to call when not all clip states exist —
    // the controller silently ignores transitions to unregistered states.
    for (auto* ae : animatedEntities) {
        if (ae && ae->controller && ae->isLocalPlayer) {
            ae->controller->setupDefaultTransitions(
                []() { return InputMaster::isActionDown("MoveForward") && !InputMaster::isKeyDown(LeftShift); },
                []() { return InputMaster::isActionDown("MoveForward") &&  InputMaster::isKeyDown(LeftShift); },
                []() { return InputMaster::isActionDown("Jump"); });
        }
    }

    if (!animatedEntities.empty())
        std::cout << "[Engine] " << animatedEntities.size() << " animated character(s) ready.\n";
    else
        std::cout << "[Engine] No animated_character entries in scene.cfg.\n";

    // Phase 5.4 — Data-driven instanced rendering via InstancedModelManager.
    // Scans PrefabManager for every prefab with "render_mode": "instanced"
    // and creates an InstancedModel bucket for each.  No hardcoded model types.
    {
        instancedModelManager = new InstancedModelManager();
        instancedModelManager->init(loader);
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
    // Phase 5.4 — Data-driven network spawning.
    //
    // Instead of blindly cloning the local player's model for every remote
    // entity, we look up the prefab for the given modelType and use the
    // prefab data to decide how to spawn the entity.
    //
    // The EntityFactory creates the ECS entity with TransformComponent and
    // other data-driven components.  We still create a visual Entity* for
    // the legacy rendering pipeline because NetworkSystem tracks Entity*
    // pointers for transform interpolation.

    // 1. Create the ECS entity from prefab data.
    EntityFactory::spawn(registry, modelType, position, physicsSystem);

    // 2. Determine whether this entity type uses an animated character model.
    const auto& prefab = PrefabManager::get().getPrefab(modelType);
    bool isAnimated = !prefab.is_null() && prefab.value("animated", false);

    // 3. Create a visual Entity* for the legacy rendering + interpolation pipeline.
    TexturedModel* remoteModel = nullptr;
    glm::vec3 modelMin(-0.5f, 0.0f, -0.5f);
    glm::vec3 modelMax( 0.5f, 2.0f,  0.5f);
    float remoteScale = 1.0f;

    if (isAnimated && player && player->getModel()) {
        // For animated entities (remote players, NPCs), reuse the local
        // player's TexturedModel as a proxy.  The actual visual comes from
        // the AnimatedEntity paired below.  Match the local player's scale
        // (usually 0) so the proxy mesh stays invisible.
        remoteModel = player->getModel();
        remoteScale = player->getScale();
    } else if (!prefab.is_null() && prefab.contains("model")) {
        // Load model from prefab data.
        const auto& modelBlock = prefab["model"];
        std::string objFile = modelBlock.value("obj", "");
        std::string texFile = modelBlock.value("texture", "");

        if (!objFile.empty()) {
            ModelData meshData = OBJLoader::loadObjModel(objFile);
            if (!meshData.getIndices().empty()) {
                auto* tex = new ModelTexture(texFile, PNG, Material{2.0f, 2.0f});
                remoteModel = new TexturedModel(loader->loadToVAO(meshData), tex);
                modelMin = meshData.getMin();
                modelMax = meshData.getMax();
            }
        }
    }

    // Fallback: use a simple OBJ model.
    if (!remoteModel) {
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

    auto* ent = new Entity(registry, remoteModel, bb, position, glm::vec3(0.0f), remoteScale);

    // Emplace the ECS NetworkSyncData so NetworkInterpolationSystem can drive it.
    registry.emplace<NetworkSyncData>(ent->getHandle());

    entities.push_back(ent);

    // Register with ChunkManager so the StreamingSystem includes this entity
    // in the active render list.
    if (chunkManager) {
        chunkManager->registerEntity(ent, position);
    }

    // Create an AnimatedEntity for animated entity types (remote players, NPCs).
    // Shares the local player's AnimatedModel — safe because AnimatedRenderer
    // uploads bone matrices per-entity before each draw call.
    if (isAnimated && !animatedEntities.empty() &&
        animatedEntities[0] && animatedEntities[0]->model) {
        AnimatedModel* sharedModel = animatedEntities[0]->model;

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

        auto* remoteAe         = new AnimatedEntity();
        remoteAe->model        = sharedModel;
        remoteAe->controller   = remoteCtrl;
        remoteAe->position     = position;
        remoteAe->scale        = animatedEntities[0]->scale;
        remoteAe->modelOffset  = animatedEntities[0]->modelOffset;
        remoteAe->isLocalPlayer = false;
        remoteAe->pairedEntity  = ent;
        animatedEntities.push_back(remoteAe);
    }

    std::cout << "[Engine] Remote entity " << networkId
              << " (model=\"" << modelType << "\") spawned at ("
              << position.x << ", " << position.y << ", " << position.z
              << ") — entities.size()=" << entities.size() << "\n";

    return ent;
}

void Engine::onNetworkDespawn(uint32_t /*networkId*/, Entity* e) {
    // Remove the paired AnimatedEntity (if any) before deleting the Entity so
    // AnimationSystem no longer renders or accesses the dangling pairedEntity ptr.
    for (auto it = animatedEntities.begin(); it != animatedEntities.end(); ++it) {
        if (*it && (*it)->pairedEntity == e) {
            delete (*it)->controller;  // Controller is owned per remote entity
            delete *it;
            animatedEntities.erase(it);
            break;
        }
    }

    // Remove from the entities list so RenderSystem stops drawing it.
    auto it = std::find(entities.begin(), entities.end(), e);
    if (it != entities.end()) {
        entities.erase(it);
    }

    // Deregister from ChunkManager so the StreamingSystem no longer
    // returns this entity in the active list.
    if (chunkManager) {
        chunkManager->removeEntity(e);
    }

    // Free the entity to prevent memory leaks.
    delete e;
}

void Engine::buildSystems() {
    // Systems are updated in this order each frame:
    //   1. InputDispatcher — translate raw InputMaster state into EventBus events
    //   2. PhysicsSystem   — step simulation, sync transforms
    //   3. InputSystem     — camera movement, picker, GUI animations
    //   4. StreamingSystem — update chunk loading, refresh entity/terrain lists
    //   5. NetworkSystem   — ENet packet polling + NetworkSyncData interpolation
    //   6. RenderSystem    — FBO + main scene render (frustum-culled)
    //   7. AnimationSystem — sync positions + animated character render
    //   8. UISystem        — object picking + UiMaster render + constraints

    // InputDispatcher must run first so that PlayerMoveCommandEvent subscribers
    // (e.g. PlayerMovementSystem) have up-to-date speed values before any other system runs.
    systems.push_back(std::make_unique<InputDispatcher>(picker));

    // Subscribe the ECS InputStateComponent to the EventBus so
    // PlayerMovementSystem uses event-driven movement instead of polling.
    if (player) {
        auto* isc = registry.try_get<InputStateComponent>(player->getHandle());
        if (isc) {
            isc->useEventBus = true;
        }
    }

    if (physicsSystem) {
        systems.push_back(std::unique_ptr<ISystem>(physicsSystem));
    }

    systems.push_back(std::make_unique<InputSystem>(
        playerCamera, primaryTerrain, picker, sampleModifiedGui, pNameText));

    // PlayerMovementSystem — ECS replacement for InputComponent::update().
    // Runs after InputSystem (camera) and PhysicsSystem, reads InputStateComponent.
    // init() subscribes to PlayerMoveCommandEvent on the EventBus.
    {
        auto pms = std::make_unique<PlayerMovementSystem>(registry);
        pms->init();
        systems.push_back(std::move(pms));
    }

    // Build the chunk manager from the first loaded terrain's texture config.
    // Initial scene entities are registered so they appear inside their chunk.

    if (primaryTerrain) {
        chunkManager = new ChunkManager(loader,
                                        primaryTerrain->getTexturePack(),
                                        primaryTerrain->getBlendMap(),
                                        terrainHeightmapFile);

        // GEA Phase 5.4 — Wire the ChunkManager's spawn callback.
        // When a chunk streams in, each baked entity is routed here:
        //   - If the prefab has "render_mode": "instanced", push its
        //     transform matrix into the InstancedModelManager.
        //   - Otherwise, create a standard ECS entity via EntityFactory.
        if (instancedModelManager) {
            chunkManager->setEntityCallback(
                [this](const BakedEntity& be, int cx, int cz) {
                    // Use the well-known BakedPrefab mapping (ChunkData.h)
                    std::string alias = BakedPrefab::toAlias(be.prefabId);
                    if (alias.empty()) {
                        return;
                    }

                    // Check if this alias should be instanced.
                    if (instancedModelManager->hasAlias(alias)) {
                        int64_t chunkKey = (static_cast<int64_t>(cx) << 32)
                                         | static_cast<int64_t>(static_cast<uint32_t>(cz));

                        glm::mat4 transform = Maths::createTransformationMatrix(
                            glm::vec3(be.x, be.y, be.z),
                            glm::vec3(0.0f, be.rotationY, 0.0f),
                            be.scale);

                        instancedModelManager->addInstance(alias, chunkKey, transform);

                    } else {
                        // Non-instanced baked entity — spawn as regular ECS entity
                        glm::vec3 pos(be.x, be.y, be.z);
                        EntityFactory::spawn(registry, alias, pos, physicsSystem);
                    }
                });

            chunkManager->setUnloadCallback(
                [this](int cx, int cz) {
                    int64_t chunkKey = (static_cast<int64_t>(cx) << 32)
                                     | static_cast<int64_t>(static_cast<uint32_t>(cz));
                    instancedModelManager->removeChunk(chunkKey);
                });
        }

        // Register existing terrain tiles so ChunkManager tracks them.
        for (auto* t : allTerrains) {
            if (t) chunkManager->registerTerrain(t);
        }
        // Register entities into the chunk grid.
        for (auto* e : entities) {
            if (e) chunkManager->registerEntity(e, e->getPosition());
        }
        systems.push_back(std::make_unique<StreamingSystem>(
            chunkManager, player, allTerrains, entities));
    }

    // Phase 4 Step 4.2 — OriginShiftSystem: prevents float-precision jitter
    // when the camera is far from the world origin.  Runs after streaming
    // (so newly loaded chunks have correct positions) and before rendering.
    {
        auto oss = std::make_unique<OriginShiftSystem>(registry);
        originShiftSystem_ = oss.get();
        systems.push_back(std::move(oss));
    }

    // NetworkSystem connects to the headless server via ENet and pushes
    // snapshots into the NetworkSyncData ECS component.  The local Player entity IS the
    // network entity — no separate dummy entity is needed.
    {
        auto netSys = std::make_unique<NetworkSystem>(
            registry,
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

        netSys->setPhysicsSystem(physicsSystem);

        netSys->init();

        networkSystem_ = netSys.get();
        systems.push_back(std::move(netSys));
    }

    // NetworkInterpolationSystem — ECS replacement for legacy interpolation.
    // Runs after NetworkSystem has pushed snapshots, before RenderSystem draws.
    systems.push_back(std::make_unique<NetworkInterpolationSystem>(registry));

    systems.push_back(std::make_unique<RenderSystem>(
        renderer, reflectFbo, entities, allTerrains, lights,
        registry, playerCamera, renderer->getProjectionMatrix(), instancedModelManager));

    systems.push_back(std::make_unique<AnimationSystem>(
        animRenderer, animatedEntities, player, lights,
        playerCamera, renderer->getProjectionMatrix()));

    systems.push_back(std::make_unique<UISystem>(
        renderer, entities, clickColorText, masterContainer,
        guiRenderer, guis));
}

void Engine::run() {
    while (DisplayManager::stayOpen()) {
        float dt = DisplayManager::getFrameTimeSeconds();

        // Phase 4 Step 4.2 — Drive the OriginShiftSystem with the camera
        // position each frame.  The shift is applied to all entity transforms
        // so render-space float precision stays tight near (0,0,0).
        if (originShiftSystem_ && playerCamera) {
            originShiftSystem_->update(playerCamera->getPosition());
        }

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

    if (instancedModelManager) {
        instancedModelManager->cleanup();
        delete instancedModelManager;
        instancedModelManager = nullptr;
    }

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
    for (auto* ae : animatedEntities) {
        if (ae) {
            // Only delete the model when this entity owns it.  Remote entities
            // share the local player's AnimatedModel pointer; deleting it more
            // than once causes the malloc double-free crash seen on quit.
            if (ae->ownsModel && ae->model) {
                ae->model->cleanUp();
                delete ae->model;
            }
            delete ae->controller;
            delete ae;
        }
    }
    animatedEntities.clear();
    delete animRenderer;
    loader->cleanUp();
    DisplayManager::closeDisplay();

    // Tear down ENet after all systems (including NetworkSystem) are destroyed.
    enet_deinitialize();
}

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
#include "EditorSystem.h"
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
#include "../ECS/Components/AnimatedModelComponent.h"
#include "../ECS/Components/StaticModelComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/ColliderComponent.h"
#include "../BoundingBox/BoundingBox.h"
#include "../Toolbox/Maths.h"
#include "../Config/ConfigManager.h"
#include "../Config/PrefabManager.h"
#include "../Config/EntityFactory.h"
#include <enet/enet.h>
#include <thread>
#include <fstream>
#include <algorithm>
#include <unordered_set>

// ImGui
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

// ---------------------------------------------------------------------------

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::init() {
    // --- VFS initialization (must happen before any file I/O) ---
    FileSystem::initVFS();

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

    // initImGui() must be called AFTER loadScene() (which creates PlayerCamera
    // → CameraInput → InputMaster::init() → installs glfwSetMouseButtonCallback
    // and glfwSetKeyCallback).  With install_callbacks=true, ImGui_ImplGlfw
    // wraps those callbacks so ImGui receives events first and chains to the
    // engine's handlers.  Calling initImGui() before loadScene() caused
    // InputMaster::init() to overwrite ImGui's mouse-button callback without
    // chaining, making all ImGui widget clicks non-functional.
    initImGui();

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

void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    // Install GLFW callbacks (chaining = true preserves existing GLFW callbacks).
    ImGui_ImplGlfw_InitForOpenGL(DisplayManager::window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void Engine::shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Engine::initFonts() {
    TextMaster::init(loader);

    fontRenderer = new FontRenderer();
    arialFont  = new FontType(TextMeshData::loadFont("arial",  48));
    noodleFont = new FontType(TextMeshData::loadFont("noodle", 48));

    fontModel = loader->loadFontVAO();

    pNameText = new GUIText("Joseph Alai MCMXII", 0.5f, fontModel, arialFont,
                            glm::vec2(540.0f, 50.0f), ColorName::Cyan,
                            0.75f * static_cast<float>(DisplayManager::Width()), false);
    texts.push_back(pNameText);
    // Remove from TextMaster's render list so the name does not appear on screen.
    TextMaster::remove(pNameText);

    clickColorText = new GUIText("Color: ", 0.5f, fontModel, arialFont,
                                 glm::vec2(10.0f, 20.0f), ColorName::Green,
                                 0.75f * static_cast<float>(DisplayManager::Width()), false);
    texts.push_back(clickColorText);
    // Remove from TextMaster's render list so the "Color:" label does not appear on screen.
    TextMaster::remove(clickColorText);
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
                              lights,
                              allTerrains, guis, texts, waterTiles,
                              primaryTerrain, player, playerCamera,
                              physicsBodyCfgs, physicsGroundCfgs);
            if (!sceneLoaded) {
                std::cerr << "[Engine] scene.json load failed — falling back to scene.cfg\n";
            }
        }
    }
    if (!sceneLoaded) {
        // Legacy .cfg fallback: SceneLoader still outputs Entity* and AnimatedEntity* objects.
        // Promote Entity* to ECS StaticModelComponent, AnimatedEntity* to AnimatedModelComponent.
        std::vector<Entity*>         legacyEntities;
        std::vector<AnimatedEntity*> legacyAnimated;
        sceneLoaded = SceneLoader::load(cfgPath, loader,
                          registry,
                          legacyEntities, lights,
                          allTerrains, guis, texts, waterTiles,
                          primaryTerrain, player, playerCamera,
                          legacyAnimated,
                          physicsBodyCfgs, physicsGroundCfgs);
        // Promote legacy Entity* objects to ECS StaticModelComponent entities.
        for (auto* ent : legacyEntities) {
            if (!ent || dynamic_cast<Player*>(ent)) continue; // Player handled separately
            // Entity* already emplaced TransformComponent + RenderComponent + ColliderComponent.
            // Add StaticModelComponent on the same ECS handle for the new render path.
            entt::entity handle = ent->getHandle();
            if (!registry.all_of<StaticModelComponent>(handle)) {
                auto& smc       = registry.emplace<StaticModelComponent>(handle);
                smc.model       = ent->getModel();
                smc.boundingBox = ent->getBoundingBox();
                smc.textureIndex = 0;
            }
            // Transfer ownership: delete the Entity* wrapper (the ECS entity survives)
            delete ent;
        }
        // Promote legacy AnimatedEntity* objects to ECS AnimatedModelComponent entities.
        for (auto* ae : legacyAnimated) {
            if (!ae) continue;
            entt::entity animEnt = registry.create();
            auto& tc    = registry.emplace<TransformComponent>(animEnt);
            tc.position = ae->position;
            tc.rotation = ae->rotation;
            tc.scale    = ae->scale;
            auto& amc       = registry.emplace<AnimatedModelComponent>(animEnt);
            amc.model       = ae->model;       // transfer ownership
            amc.controller  = ae->controller;
            amc.modelOffset = ae->modelOffset;
            amc.scale       = ae->scale;
            amc.ownsModel   = true;
            amc.isLocalPlayer = false;  // marked below
            // Default to the loader's coordinate correction so Y-up auto-correction
            // is preserved when AnimatedRenderer no longer applies coordinateCorrection.
            amc.modelRotationMat = ae->model->coordinateCorrection;
            ae->model      = nullptr;   // prevent double-delete
            ae->controller = nullptr;
            delete ae;
        }
    }

    // All animated entities loaded at startup belong to the local player's
    // character(s).  Mark them so AnimationSystem syncs their position from
    // the physics-driven Player* rather than from NetworkInterpolationSystem.
    {
        auto animView = registry.view<AnimatedModelComponent>();
        for (auto e : animView) {
            auto& amc     = animView.get<AnimatedModelComponent>(e);
            amc.isLocalPlayer = true;
        }
        if (!animView.empty())
            std::cout << "[Engine] " << animView.size()
                      << " animated character(s) ready.\n";
    }

    // Set up physics world and register bodies from config
    physicsSystem = new PhysicsSystem();
    physicsSystem->setRegistry(registry);  // Bind ECS registry for TransformComponent sync
    physicsSystem->init();

    // Add ground planes
    for (const auto& g : physicsGroundCfgs) {
        physicsSystem->addGroundPlane(g.yHeight);
    }

    // Add rigid bodies for named entities.
    // SceneLoaderJson path: uses entityHandle (direct ECS handle).
    // Legacy SceneLoader path: entityHandle is set when promoting Entity* to StaticModelComponent.
    for (const auto& cfg : physicsBodyCfgs) {
        entt::entity handle = cfg.entityHandle;
        if (handle == entt::null) continue;
        if (!registry.valid(handle)) continue;

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
                physicsSystem->addStaticBody(handle, def.shape, def.halfExtents,
                                             def.friction, def.restitution);
                break;
            case BodyType::Kinematic:
                physicsSystem->addKinematicBody(handle, def);
                break;
            case BodyType::Dynamic:
            default:
                physicsSystem->addDynamicBody(handle, def);
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
        isc.allTerrains   = &allTerrains;
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
    // sampleModifiedGui is kept for InputSystem animation but is NOT added to guis
    // so it is never rendered on screen.
    sampleModifiedGui = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(),
                                       glm::vec2(-0.72f, 0.3f),
                                       glm::vec2(0.290f, 0.0900f) / 3.0f);

    // Clear the lifebar / green-bar / heart textures that SceneLoader loaded from
    // scene.json so they are not rendered by the raw GUI renderer.
    guis.clear();

    masterContainer = UiMaster::getMasterComponent();
    masterContainer->setName("Master Container");

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

    // Create the entity picker for Ray-AABB picking (Part 1: Client-Side Object Picking).
    entityPicker_ = new EntityPicker(registry);

    // Wire movement-driven animation transitions for every local-player character.
    // setupDefaultTransitions is safe to call when not all clip states exist —
    // the controller silently ignores transitions to unregistered states.
    //
    // The walk/run conditions capture the entity handle and registry pointer so
    // they can read AnimatedModelComponent::isMoving, which AnimationSystem sets
    // each frame based on any directional key OR a non-zero position delta
    // (click-to-walk / server-authoritative auto-walk).  This ensures the Walk
    // animation fires for A, D, S, diagonals, and auto-walk — not only W.
    {
        auto animView = registry.view<AnimatedModelComponent>();
        for (auto e : animView) {
            auto& amc = animView.get<AnimatedModelComponent>(e);
            if (amc.controller && amc.isLocalPlayer) {
                entt::entity handle = e;
                entt::registry* regPtr = &registry;
                amc.controller->setupDefaultTransitions(
                    [handle, regPtr]() {
                        auto* p = regPtr->try_get<AnimatedModelComponent>(handle);
                        return p && p->isMoving && !InputMaster::isKeyDown(LeftShift);
                    },
                    [handle, regPtr]() {
                        auto* p = regPtr->try_get<AnimatedModelComponent>(handle);
                        return p && p->isMoving && InputMaster::isKeyDown(LeftShift);
                    },
                    []() { return InputMaster::isActionDown("Jump"); });
            }
        }
        if (animView.empty())
            std::cout << "[Engine] No animated_character entries in scene.cfg.\n";
    }

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

entt::entity Engine::onNetworkSpawn(uint32_t networkId,
                                    const std::string& modelType,
                                    const glm::vec3& position) {
    // Step 5.4 — Pure ECS network spawn.
    //
    // EntityFactory reads the prefab JSON for modelType and attaches the
    // appropriate components: TransformComponent, NetworkIdComponent,
    // InputStateComponent/InputQueueComponent, optional physics controller,
    // and — for animated prefabs — AnimatedModelComponent (skeleton + clips).
    //
    // NetworkInterpolationSystem drives TransformComponent each frame.
    // AnimationSystem renders AnimatedModelComponent entities via ECS view.
    // No legacy Entity* or AnimatedEntity* objects are created here.

    entt::entity e = EntityFactory::spawn(registry, modelType, position, physicsSystem);
    if (e == entt::null) {
        std::cerr << "[Engine] onNetworkSpawn: unknown prefab \"" << modelType
                  << "\" for networkId=" << networkId << "\n";
        return entt::null;
    }

    // Remote entities are driven entirely by the server's TransformSnapshots via
    // NetworkInterpolationSystem.  Bullet's CharacterController would fight that
    // system every frame, snapping the entity back to the ghost's position and
    // causing severe twitching.  Remove it so the interpolation system has sole
    // authority over the entity's position on the client.
    if (physicsSystem && physicsSystem->hasCharacterController(e)) {
        physicsSystem->removeCharacterController(e);
    }

    // Attach interpolation state so NetworkInterpolationSystem can drive this entity.
    registry.emplace<NetworkSyncData>(e);

    // Add a ColliderComponent so EntityPicker::pick() can find this entity
    // via Ray-AABB when the player clicks on it.  The AABB is derived from the
    // prefab's physics capsule dimensions and scaled to match the visual size
    // (amc.scale) so that clicking anywhere on the visible character is a hit.
    {
        const auto& prefab = PrefabManager::get().getPrefab(modelType);
        if (!prefab.is_null() && prefab.contains("physics")) {
            const auto& phys = prefab["physics"];
            float r = phys.value("radius", 0.5f);
            float h = phys.value("height", 1.8f);

            // Use the visual (AnimatedModelComponent) scale so the AABB
            // covers what the player actually sees on screen.
            float s = 1.0f;
            if (auto* amc = registry.try_get<AnimatedModelComponent>(e)) {
                s = amc->scale;
            }

            // Bottom of the AABB sits at the entity origin (ground level);
            // top at h*s — the full visual character height.
            // Previously used symmetric halfExtents which put half the box
            // underground, making the upper body impossible to click.
            auto* box = new BoundingBox(nullptr, glm::vec3(1.0f));
            box->setAABB(glm::vec3(-r * s, 0.0f,  -r * s),
                         glm::vec3( r * s, h * s,   r * s));
            registry.emplace_or_replace<ColliderComponent>(e, ColliderComponent{box});
        }
    }

    std::cout << "[Engine] Remote entity " << networkId
              << " (model=\"" << modelType << "\") spawned at ("
              << position.x << ", " << position.y << ", " << position.z << ")\n";

    return e;
}

void Engine::onNetworkDespawn(uint32_t /*networkId*/, entt::entity e) {
    if (!registry.valid(e)) return;

    // Release AnimatedModelComponent resources if present.
    if (auto* amc = registry.try_get<AnimatedModelComponent>(e)) {
        if (amc->ownsModel && amc->model) {
            amc->model->cleanUp();
            delete amc->model;
        }
        delete amc->controller;
    }

    // Release ColliderComponent bounding box if it was heap-allocated here.
    if (auto* cc = registry.try_get<ColliderComponent>(e)) {
        delete cc->box;
        cc->box = nullptr;
    }

    // Destroy all ECS components and the entity itself.
    registry.destroy(e);
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
    //   9. EditorSystem    — Dear ImGui World Editor overlay (last, owns ImGui frame)

    // InputDispatcher must run first so that PlayerMoveCommandEvent subscribers
    // (e.g. PlayerMovementSystem) have up-to-date speed values before any other system runs.
    systems.push_back(std::make_unique<InputDispatcher>(
        picker, &editorState_,
        entityPicker_, playerCamera, renderer->getProjectionMatrix(), &registry));

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
        playerCamera, primaryTerrain, picker, sampleModifiedGui, pNameText, &editorState_));

    // Build the chunk manager from the first loaded terrain's texture config.
    // Initial scene entities are registered so they appear inside their chunk.

    if (primaryTerrain) {
        chunkManager = new ChunkManager(loader,
                                        primaryTerrain->getTexturePack(),
                                        primaryTerrain->getBlendMap(),
                                        terrainHeightmapFile);

        chunkManager->setTerrainLoadCallback([this](Terrain* newlyLoadedTerrain) {
            if (this->physicsSystem) {
                this->physicsSystem->addTerrainCollider(newlyLoadedTerrain);
            }
        });

        // GEA Phase 5.4 — Wire the ChunkManager's spawn callback.
        // When a chunk streams in, each baked entity is routed here:
        //   - If the prefab has "render_mode": "instanced", push its
        //     transform matrix into the InstancedModelManager.
        //   - Otherwise, create a standard ECS entity via EntityFactory.
        if (instancedModelManager) {

            // Deterministic static ID: same algorithm as the server so both sides
            // independently generate matching IDs for each world-space object.
            // The high bit is set to avoid collision with dynamic (player/NPC) IDs.
            // A shared usedIds set (across all chunks) ensures cross-chunk
            // collisions are also resolved in the same order as the server.
            auto generateStaticId = [](float x, float z) -> uint32_t {
                uint32_t ux = static_cast<uint32_t>(std::round(x * 10.0f) + 2000000.0f) & 0x7FFF;
                uint32_t uz = static_cast<uint32_t>(std::round(z * 10.0f) + 2000000.0f) & 0x7FFF;
                return 0x80000000u | (ux << 15) | uz;
            };

            // Shared set persists for the lifetime of the Engine so collisions
            // are resolved globally, matching the server's single usedIds set.
            auto chunkUsedIds = std::make_shared<std::unordered_set<uint32_t>>();

            auto resolveStaticIdChunk = [](uint32_t candidate,
                                           std::unordered_set<uint32_t>& usedIds,
                                           float x, float z) -> uint32_t {
                if (usedIds.count(candidate)) {
                    uint32_t original = candidate;
                    do {
                        candidate = 0x80000000u | (((candidate & 0x7FFFFFFFu) + 1u) & 0x7FFFFFFFu);
                    } while (usedIds.count(candidate));
                    std::cerr << "[Engine] WARNING: Static ID collision at ("
                              << x << ", " << z << ") — 0x" << std::hex << original
                              << " -> 0x" << candidate << std::dec << "\n";
                }
                usedIds.insert(candidate);
                return candidate;
            };

            chunkManager->setEntityCallback(
                [this, generateStaticId, chunkUsedIds, resolveStaticIdChunk]
                (const BakedEntity& be, int cx, int cz) {
                    // Use the well-known BakedPrefab mapping (ChunkData.h)
                    std::string alias = BakedPrefab::toAlias(be.prefabId);
                    if (alias.empty()) {
                        return;
                    }

                    glm::vec3 pos(be.x, be.y, be.z);
                    glm::vec3 rot(0.0f, be.rotationY, 0.0f);

                    // Spawn an ECS entity (no physics on client for static objects —
                    // the server is authoritative for static collision).
                    // This gives us TransformComponent + NetworkIdComponent +
                    // InteractableComponent from the prefab, which are needed for
                    // EntityPicker to identify and publish EntityClickedEvent.
                    entt::entity e = EntityFactory::spawn(registry, alias, pos, nullptr, rot, be.scale);

                    if (e != entt::null) {
                        // Assign the deterministic static network ID with collision
                        // resolution so the client matches the server's assigned IDs.
                        uint32_t staticNetId = resolveStaticIdChunk(
                            generateStaticId(be.x, be.z), *chunkUsedIds, be.x, be.z);
                        if (auto* nid = registry.try_get<NetworkIdComponent>(e)) {
                            nid->id    = staticNetId;
                            nid->isNPC = false;
                        } else {
                            registry.emplace<NetworkIdComponent>(e,
                                NetworkIdComponent{staticNetId, alias, false, 0});
                        }

                        // Add a ColliderComponent with an AABB derived from the
                        // prefab's physics halfExtents so EntityPicker::pick() can
                        // perform Ray-AABB tests against this entity.
                        const auto& prefab = PrefabManager::get().getPrefab(alias);
                        if (!prefab.is_null() && prefab.contains("physics")) {
                            const auto& phys = prefab["physics"];
                            glm::vec3 halfExtents(0.5f);
                            if (phys.contains("halfExtents") &&
                                phys["halfExtents"].is_array() &&
                                phys["halfExtents"].size() >= 3) {
                                halfExtents = glm::vec3(
                                    phys["halfExtents"][0].get<float>(),
                                    phys["halfExtents"][1].get<float>(),
                                    phys["halfExtents"][2].get<float>()) * be.scale;
                            } else if (phys.contains("radius")) {
                                float r = phys.value("radius", 0.5f) * be.scale;
                                halfExtents = glm::vec3(r);
                            }
                            // BoundingBox with null RawBoundingBox is safe here:
                            // EntityPicker only calls getAABB(); the GPU mesh pointer
                            // is never dereferenced for ECS-only (non-Entity*) objects.
                            auto* box = new BoundingBox(nullptr, glm::vec3(1.0f));
                            box->setAABB(-halfExtents, halfExtents);
                            registry.emplace_or_replace<ColliderComponent>(e, ColliderComponent{box});
                        }
                    }

                    // Check if this alias should be instanced for rendering.
                    if (instancedModelManager->hasAlias(alias)) {
                        int64_t chunkKey = (static_cast<int64_t>(cx) << 32)
                                         | static_cast<int64_t>(static_cast<uint32_t>(cz));

                        glm::mat4 transform = Maths::createTransformationMatrix(
                            glm::vec3(be.x, be.y, be.z),
                            glm::vec3(0.0f, be.rotationY, 0.0f),
                            be.scale);

                        instancedModelManager->addInstance(alias, chunkKey, transform);

                    } else {
                        // Non-instanced baked entity — visual rendering via AssimpModelComponent
                        // or AnimatedModelComponent (already set up by EntityFactory::spawn above
                        // if the prefab has a "mesh" key).  Re-spawn only if EntityFactory didn't
                        // create an entity (e.g., prefab has no model_type).
                        if (e == entt::null) {
                            EntityFactory::spawn(registry, alias, pos, physicsSystem, rot, be.scale);
                        }
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
        systems.push_back(std::make_unique<StreamingSystem>(
            chunkManager, player, allTerrains));
    }

    // PlayerMovementSystem — ECS replacement for InputComponent::update().
    // IMPORTANT: must run AFTER StreamingSystem so that allTerrains is
    // refreshed with newly loaded chunks before terrain-height collision runs.
    // Runs after InputSystem (camera), PhysicsSystem, and StreamingSystem.
    // init() subscribes to PlayerMoveCommandEvent on the EventBus.
    {
        auto pms = std::make_unique<PlayerMovementSystem>(registry);
        pms->init();
        systems.push_back(std::move(pms));
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
            // Spawn callback — pure ECS, returns entt::entity handle
            [this](uint32_t nid, const std::string& model,
                   const glm::vec3& pos) -> entt::entity {
                return onNetworkSpawn(nid, model, pos);
            },
            // Despawn callback — destroys ECS entity + resources
            [this](uint32_t nid, entt::entity e) {
                onNetworkDespawn(nid, e);
            }
        );

        netSys->setPhysicsSystem(physicsSystem);
        if (playerCamera) netSys->setPlayerCamera(playerCamera);

        netSys->init();

        networkSystem_ = netSys.get();
        systems.push_back(std::move(netSys));
    }

    // NetworkInterpolationSystem — ECS replacement for legacy interpolation.
    // Runs after NetworkSystem has pushed snapshots, before RenderSystem draws.
    systems.push_back(std::make_unique<NetworkInterpolationSystem>(registry));

    systems.push_back(std::make_unique<RenderSystem>(
        renderer, reflectFbo, player, allTerrains, lights,
        registry, playerCamera, renderer->getProjectionMatrix(), instancedModelManager,
        &editorState_));

    systems.push_back(std::make_unique<AnimationSystem>(
        animRenderer, registry, player, lights,
        playerCamera, renderer->getProjectionMatrix()));

    systems.push_back(std::make_unique<UISystem>(
        renderer, registry, player, clickColorText, masterContainer,
        guiRenderer, guis));

    // EditorSystem must be last — it owns the ImGui frame boundary
    // (NewFrame / Render) and its ImGui draw calls must come after all other
    // render systems have submitted their geometry for this frame.
    {
        const std::string sceneJsonPath = FileSystem::Scene("scene.json");
        systems.push_back(std::make_unique<EditorSystem>(
            registry, picker, physicsSystem, renderer,
            editorState_, sceneJsonPath));
    }
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
    // Release all AnimatedModelComponent resources stored in the ECS registry.
    {
        auto animView = registry.view<AnimatedModelComponent>();
        for (auto e : animView) {
            auto& amc = animView.get<AnimatedModelComponent>(e);
            if (amc.ownsModel && amc.model) {
                amc.model->cleanUp();
                delete amc.model;
                amc.model = nullptr;
            }
            delete amc.controller;
            amc.controller = nullptr;
        }
    }
    // Release StaticModelComponent BoundingBox objects (owned per-instance).
    {
        auto staticView = registry.view<StaticModelComponent>();
        for (auto e : staticView) {
            auto& smc = staticView.get<StaticModelComponent>(e);
            delete smc.boundingBox;
            smc.boundingBox = nullptr;
        }
    }
    delete animRenderer;
    delete entityPicker_;
    entityPicker_ = nullptr;
    loader->cleanUp();
    shutdownImGui();
    DisplayManager::closeDisplay();

    // Tear down ENet after all systems (including NetworkSystem) are destroyed.
    enet_deinitialize();
    FileSystem::shutdownVFS();
}

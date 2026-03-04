//
// Created as part of Phase 0 engine refactoring.
// Splits MainGameLoop::main() into a clean Engine class with lifecycle methods.
// The game loop delegates all per-frame work to ordered ISystem instances.
//

#include "Engine.h"
#include "SceneLoader.h"
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
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"
#include "../Guis/Text/GUIText.h"
#include "../Guis/GuiComponent.h"
#include "../Guis/Constraints/UiConstraints.h"
#include "../Textures/TerrainTexture.h"
#include "../Textures/TerrainTexturePack.h"
#include "../BoundingBox/BoundingBoxIndex.h"
#include "../Toolbox/Picker.h"
#include "../Input/InputMaster.h"
#include <thread>

// ---------------------------------------------------------------------------

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::init() {
    DisplayManager::createDisplay();
    loader = new Loader();

    initFonts();
    loadScene();
    initRenderers();
    initGui();
    initFramebuffersAndPickers();
    buildSystems();
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
    // Load 3-D scene content from the text config file.
    // Developers can edit scene.cfg to add/remove/modify models, entities,
    // lights, terrain tiles, and GUI textures at runtime without recompiling.
    const std::string configPath = FileSystem::Scene("scene.cfg");

    std::vector<SceneLoader::PhysicsBodyCfg>   physicsBodyCfgs;
    std::vector<SceneLoader::PhysicsGroundCfg> physicsGroundCfgs;

    SceneLoader::load(configPath, loader,
                      entities, scenes, lights,
                      allTerrains, guis, texts, waterTiles,
                      primaryTerrain, player, playerCamera,
                      animatedEntities,
                      physicsBodyCfgs, physicsGroundCfgs);

    // Set up physics world and register bodies from config
    physicsSystem = new PhysicsSystem();
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

    // Set up a kinematic character controller for the player so Bullet handles
    // gravity and collision instead of the manual terrain-height fallback.
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

    // Water renderer — loads optional DuDv / normal textures, falls back to neutral 1×1 textures
    if (!waterTiles.empty()) {
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
        for (const auto& tile : waterTiles)
            renderer->addWaterTile(tile);
    }

    picker = new TerrainPicker(playerCamera, renderer->getProjectionMatrix(), primaryTerrain);

    for (auto e : entities) {
        if (e->getBoundingBox() != nullptr) {
            allBoxes.push_back(e);
        }
    }
    allBoxes.reserve(entities.size() + scenes.size());
    allBoxes.insert(allBoxes.end(), entities.begin(), entities.end());
    allBoxes.insert(allBoxes.end(), scenes.begin(), scenes.end());

    // Wire keyboard-driven animation transitions for every animated character.
    // SceneLoader registered no-op lambdas; replace with real input-driven ones.
    // setupDefaultTransitions is safe to call when not all clip states exist —
    // the controller silently ignores transitions to unregistered states.
    for (auto* ae : animatedEntities) {
        if (ae && ae->controller) {
            ae->controller->setupDefaultTransitions(
                []() { return InputMaster::isKeyDown(W) && !InputMaster::isKeyDown(LeftShift); },
                []() { return InputMaster::isKeyDown(W) &&  InputMaster::isKeyDown(LeftShift); },
                []() { return InputMaster::isKeyDown(Space); });
        }
    }
}

void Engine::buildSystems() {
    // Systems are updated in this order each frame:
    //   1. InputDispatcher — translate raw InputMaster state into EventBus events
    //   2. PhysicsSystem   — step simulation, sync transforms
    //   3. InputSystem     — camera movement, picker, GUI animations
    //   4. StreamingSystem — update chunk loading, refresh entity/terrain lists
    //   5. RenderSystem    — FBO + main scene render (frustum-culled)
    //   6. AnimationSystem — sync positions + animated character render
    //   7. UISystem        — object picking + UiMaster render + constraints

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
        playerCamera, primaryTerrain, picker, sampleModifiedGui, pNameText));

    // Build the chunk manager from the first loaded terrain's texture config.
    // Initial scene entities are registered so they appear inside their chunk.
    if (primaryTerrain) {
        chunkManager = new ChunkManager(loader,
                                        primaryTerrain->getTexturePack(),
                                        primaryTerrain->getBlendMap(),
                                        terrainHeightmapFile);
        // Register existing terrain tiles so ChunkManager tracks them.
        for (auto* t : allTerrains) {
            if (t) chunkManager->registerTerrain(t);
        }
        // Register entities into the chunk grid.
        for (auto* e : entities) {
            if (e) chunkManager->registerEntity(e, e->getPosition());
        }
        for (auto* s : scenes) {
            if (s) chunkManager->registerAssimpEntity(s, s->getPosition());
        }
        systems.push_back(std::make_unique<StreamingSystem>(
            chunkManager, player, allTerrains, entities, scenes));
    }

    systems.push_back(std::make_unique<RenderSystem>(
        renderer, reflectFbo, entities, scenes, allTerrains, lights, allBoxes,
        playerCamera, renderer->getProjectionMatrix()));

    systems.push_back(std::make_unique<AnimationSystem>(
        animRenderer, animatedEntities, player, lights,
        playerCamera, renderer->getProjectionMatrix()));

    systems.push_back(std::make_unique<UISystem>(
        renderer, allBoxes, clickColorText, fontModel, noodleFont, masterContainer));
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
            if (ae->model) { ae->model->cleanUp(); delete ae->model; }
            delete ae->controller;
            delete ae;
        }
    }
    animatedEntities.clear();
    delete animRenderer;
    loader->cleanUp();
    DisplayManager::closeDisplay();
}

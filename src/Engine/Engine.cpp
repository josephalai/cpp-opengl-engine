//
// Created as part of Phase 0 engine refactoring.
// Splits MainGameLoop::main() into a clean Engine class with lifecycle methods.
//

#include "Engine.h"
#include "../Util/FileSystem.h"
#include "../Util/Utils.h"
#include "../Util/LightUtil.h"
#include "../Util/ColorName.h"
#include "../Util/CommonHeader.h"
#include "../RenderEngine/DisplayManager.h"
#include "../RenderEngine/ObjLoader.h"
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"
#include "../Guis/Text/FontMeshCreator/GUIText.h"
#include "../Guis/GuiComponent.h"
#include "../Guis/UiConstraints.h"
#include "../Textures/TerrainTexture.h"
#include "../Textures/TerrainTexturePack.h"
#include "../BoundingBox/BoundingBoxIndex.h"
#include "../Toolbox/Picker.h"
#include "../Input/InputMaster.h"
#include <thread>

// --- Scene-setup utility helpers (free functions, local to this file) ---

static glm::vec3 generateRandomPosition(Terrain *terrain, float yOffset = 0.0f) {
    glm::vec3 positionVector(0.0f);
    positionVector.x = floor(Utils::randomFloat() * 1500 - 800);
    positionVector.z = floor(Utils::randomFloat() * -800);
    positionVector.y = terrain->getHeightOfTerrain(positionVector.x, positionVector.z) + yOffset;
    return positionVector;
}

static glm::vec3 generateRandomRotation() {
    float rx = 0;
    float ry = Utils::randomFloat() * 100 - 50;
    float rz = 0;
    glm::vec3 rot(rx, ry, rz);
    rot = rot * 180.0f;
    return rot;
}

static float generateRandomScale(float min = 0.75f, float max = 1.50f) {
    float multiplier = 1;
    if (max > 1) {
        multiplier = ceil(max);
    }
    auto r = Utils::randomFloat() * multiplier;
    if (r < min) {
        r = min;
    }
    if (r > max) {
        r = max;
    }
    return r;
}

// ---------------------------------------------------------------------------

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::init() {
    DisplayManager::createDisplay();
    loader = new Loader();

    initFonts();
    loadModels();
    initTerrain();
    spawnEntities();
    initPlayer();
    initLights();
    initRenderers();
    initGui();
    initFramebuffersAndPickers();
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

void Engine::loadModels() {
    ModelData lampData,       fernData,     grassData;
    ModelData stallData,      treeData,     fluffyTreeData;
    BoundingBoxData lampBbData, fernBbData, grassBnData;
    BoundingBoxData stallBbData, treeBbData, fluffyTreeBbData;

    auto f = [](ModelData *pModelData, BoundingBoxData *pBbData, const std::string &filename) {
        *pModelData = OBJLoader::loadObjModel(filename);
        *pBbData    = OBJLoader::loadBoundingBox(*pModelData, ClickBoxTypes::BOX, BoundTypes::AABB);
    };

    std::vector<const std::string>   modelFiles = {"lamp", "fern", "grassModel", "Stall", "tree", "fluffy-tree"};
    std::vector<ModelData *>         modelDatas = {&lampData, &fernData, &grassData, &stallData, &treeData, &fluffyTreeData};
    std::vector<BoundingBoxData *>   bbDatas    = {&lampBbData, &fernBbData, &grassBnData, &stallBbData, &treeBbData, &fluffyTreeBbData};

    std::vector<std::thread> vThreads;
    for (int i = 0; i < (int)modelDatas.size(); ++i) {
        vThreads.emplace_back(f, modelDatas[i], bbDatas[i], modelFiles[i]);
    }
    for (auto &&t : vThreads) {
        t.join();
    }

    pLampBox        = loader->loadToVAO(lampBbData);
    staticLamp      = new TexturedModel(loader->loadToVAO(lampData),       new ModelTexture("lamp",         PNG));

    pFernBox        = loader->loadToVAO(fernBbData);
    staticFern      = new TexturedModel(loader->loadToVAO(fernData),       new ModelTexture("fern",         PNG));
    staticFern->getModelTexture()->setNumberOfRows(2);

    pGrassBox       = loader->loadToVAO(grassBnData);
    auto grassTexture = new ModelTexture("grassTexture", PNG, true, true);
    staticGrass     = new TexturedModel(loader->loadToVAO(grassData),      grassTexture);

    const Material material = Material{.shininess = 2.0f, .reflectivity = 2.0f};

    pStallBox       = loader->loadToVAO(stallBbData);
    staticStall     = new TexturedModel(loader->loadToVAO(stallData),      new ModelTexture("stallTexture", PNG, material));

    pTreeBox        = loader->loadToVAO(treeBbData);
    staticTree      = new TexturedModel(loader->loadToVAO(treeData),       new ModelTexture("tree",         PNG, material));

    pFluffyTreeBox  = loader->loadToVAO(fluffyTreeBbData);
    staticFluffyTree = new TexturedModel(loader->loadToVAO(fluffyTreeData), new ModelTexture("tree",         PNG, material));

    // Keep stallData for initPlayer (load player model from same data)
    // Store the player raw model here so initPlayer can reuse stallData
    // Note: we reload stall for the player separately in initPlayer()
    // We need stallData available in initPlayer, so store it as the player model
    // is loaded from the same obj file.
}

void Engine::initTerrain() {
    auto backgroundTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/grass")->getId());
    auto rTexture          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/dirt")->getId());
    auto gTexture          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blueflowers")->getId());
    auto bTexture          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/brickroad")->getId());
    auto texturePack       = new TerrainTexturePack(backgroundTexture, rTexture, gTexture, bTexture);
    auto blendMap          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blendMap")->getId());

    primaryTerrain = new Terrain(0, -1, loader, texturePack, blendMap, "heightMap");

    allTerrains.push_back(new Terrain(-1, -1, loader, texturePack, blendMap, "heightMap"));
    allTerrains.push_back(new Terrain(0,   0, loader, texturePack, blendMap, "heightMap"));
    allTerrains.push_back(new Terrain(-1,  0, loader, texturePack, blendMap, "heightMap"));
    allTerrains.push_back(primaryTerrain);
}

void Engine::spawnEntities() {
    // Load assimp model
    auto *pBackpack     = new AssimpMesh("Backpack/backpack");
    auto  pBackpackBox  = OBJLoader::loadBoundingBox(pBackpack, ClickBoxTypes::BOX, BoundTypes::AABB);
    auto  pBackpackBoxs = loader->loadToVAO(pBackpackBox);
    scenes.push_back(new AssimpEntity(pBackpack,
                                      new BoundingBox(pBackpackBoxs, BoundingBoxIndex::genUniqueId()),
                                      generateRandomPosition(primaryTerrain, 3.0f),
                                      generateRandomRotation(),
                                      generateRandomScale(3.25f, 10.50f)));

    // Static entities
    auto lampy = new Entity(staticLamp,
                            new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                            glm::vec3(120.0f, primaryTerrain->getHeightOfTerrain(120, -50), -50.0f));
    entities.push_back(lampy);
    entities.push_back(new Entity(staticStall,
                                  new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                                  glm::vec3(1.0f, 0.0f, -82.4f),
                                  glm::vec3(0.0f, 180.0f, 0.0f)));
    entities.push_back(new Entity(staticLamp,
                                  new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                                  glm::vec3(100.0f, primaryTerrain->getHeightOfTerrain(100, -50), -50.0f)));
    entities.push_back(new Entity(staticLamp,
                                  new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                                  glm::vec3(110.0f, primaryTerrain->getHeightOfTerrain(110, -20), -20.0f)));

    // Random spawns
    for (int i = 0; i < 500; ++i) {
        entities.push_back(new Entity(staticGrass,
                                      new BoundingBox(pGrassBox, BoundingBoxIndex::genUniqueId()),
                                      generateRandomPosition(primaryTerrain),
                                      generateRandomRotation(),
                                      generateRandomScale(0.5f, 1.50f)));
        entities.push_back(new Entity(staticFluffyTree,
                                      new BoundingBox(pFluffyTreeBox, BoundingBoxIndex::genUniqueId()),
                                      generateRandomPosition(primaryTerrain),
                                      generateRandomRotation(),
                                      generateRandomScale(0.5f, 1.50f)));
        entities.push_back(new Entity(staticTree,
                                      new BoundingBox(pTreeBox, BoundingBoxIndex::genUniqueId()),
                                      generateRandomPosition(primaryTerrain),
                                      generateRandomRotation(),
                                      generateRandomScale(.25f, 1.50f)));
        entities.push_back(new Entity(staticFern,
                                      new BoundingBox(pFernBox, BoundingBoxIndex::genUniqueId()),
                                      Utils::roll(1, 4),
                                      generateRandomPosition(primaryTerrain),
                                      generateRandomRotation(),
                                      generateRandomScale(.25f, 1.50f)));
    }
}

void Engine::initPlayer() {
    ModelData stallData   = OBJLoader::loadObjModel("Stall");
    auto playerModel      = loader->loadToVAO(stallData);
    auto playerOne        = new TexturedModel(playerModel, new ModelTexture("stallTexture", PNG));
    auto stallBbData      = OBJLoader::loadBoundingBox(stallData, ClickBoxTypes::BOX, BoundTypes::AABB);
    auto playerStallBox   = loader->loadToVAO(stallBbData);

    player = new Player(playerOne,
                        new BoundingBox(playerStallBox, BoundingBoxIndex::genUniqueId()),
                        glm::vec3(100.0f, 3.0f, -50.0f),
                        glm::vec3(0.0f, 180.0f, 0.0f), 1.0f);
    InteractiveModel::setInteractiveBox(player);
    entities.push_back(player);

    playerCamera = new PlayerCamera(player);
}

void Engine::initLights() {
    auto d = LightUtil::AttenuationDistance(65);
    Lighting l = Lighting{glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0.5f, 0.5f, 0.5f), d.x, d.y, d.z};

    lights.push_back(new Light(glm::vec3(0.0f, 1000.0f, -7000.0f), glm::vec3(0.4f, 0.4f, 0.4f), {
        .ambient  = glm::vec3(0.2f, 0.2f, 0.2f),
        .diffuse  = glm::vec3(0.5f, 0.5f, 0.5f),
        .constant = Light::kDirectional,
    }));
    lights.push_back(new Light(
        glm::vec3(120.0f, primaryTerrain->getHeightOfTerrain(120, -50) + 10, -50.0f),
        glm::vec3(0.0f, 0.0f, 2.0f), l));
    lights.push_back(new Light(
        glm::vec3(100.0f, primaryTerrain->getHeightOfTerrain(100, -50) + 10, -50.0f),
        glm::vec3(2.0f, 0.0f, 0.0f), l));
    lights.push_back(new Light(
        glm::vec3(110.0f, primaryTerrain->getHeightOfTerrain(110, -20) + 10, -20.0f),
        glm::vec3(0.0f, 2.0f, 0.0f), l));
}

void Engine::initRenderers() {
    renderer    = new MasterRenderer(playerCamera, loader);
    guiRenderer = new GuiRenderer(loader);
    rectRenderer = new RectRenderer(loader);

    UiMaster::initialize(loader, guiRenderer, fontRenderer, rectRenderer);
}

void Engine::initGui() {
    auto t1 = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(),
                             glm::vec2(-0.72f, 0.9f), glm::vec2(0.290f, 0.0900f));
    guis.push_back(t1);
    auto t2 = new GuiTexture(loader->loadTexture("gui/green")->getId(),
                             glm::vec2(-0.7f, 0.9f), glm::vec2(0.185f, 0.070f));
    guis.push_back(t2);
    auto t3 = new GuiTexture(loader->loadTexture("gui/heart")->getId(),
                             glm::vec2(-0.9f, 0.9f), glm::vec2(0.075f, 0.075f));
    guis.push_back(t3);

    sampleModifiedGui = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(),
                                       glm::vec2(-0.72f, 0.3f),
                                       glm::vec2(0.290f, 0.0900f) / 3.0f);
    guis.push_back(sampleModifiedGui);

    glm::vec3 color  = glm::vec3(ColorName::Cyan.getR(),  ColorName::Cyan.getG(),  ColorName::Cyan.getB());
    glm::vec2 position  = glm::vec2(-0.75f, 0.67f);
    glm::vec2 size      = glm::vec2(0.290f, 0.0900f);
    glm::vec2 scale     = glm::vec2(0.25f,  0.33f);
    float     alpha     = 0.33f;

    auto guiRect  = new GuiRect(color, position, size, scale, alpha);
    glm::vec2 position2 = glm::vec2(-0.55f, 0.37f);
    glm::vec3 color2    = glm::vec3(ColorName::Green.getR(), ColorName::Green.getG(), ColorName::Green.getB());
    auto guiRect2 = new GuiRect(color2, position2, size, scale, alpha);
    rects.push_back(guiRect);

    sampleModifiedGui->addChild(sampleModifiedGui, new UiConstraints(0, 0, 200, 200));

    masterContainer = UiMaster::getMasterComponent();
    auto parent     = new GuiComponent(Container::CONTAINER, new UiConstraints(0.01f, -0.01f, 50, 50));
    parent->setName("Parent");
    masterContainer->setName("Master Container");

    t1->setName("gui/lifebar");
    t2->setName("gui/green");
    t3->setName("gui/heart");
    guiRect->setName("GuiRect");
    guiRect2->setName("GuiRect2");

    masterContainer->addChild(guiRect,   new UiConstraints(0.0f,  -0.1f, 50, 50));
    masterContainer->addChild(guiRect2,  new UiConstraints(0.0f,  -0.1f, 50, 50));
    masterContainer->addChild(parent,    new UiConstraints(0.02f, -0.1f, 50, 50));
    parent->addChild(t1,          new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(t2,          new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(t3,          new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(texts[0],    new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(texts[1],    new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(clickColorText, new UiConstraints(0.00f, -0.1f, 50, 50));
    t1->addChild(pNameText,       new UiConstraints(-500.00f, 40.1f, 50, 50));

    masterContainer->initialize();
    UiMaster::createRenderQueue(masterContainer);
    UiMaster::applyConstraints(masterContainer);
}

void Engine::initFramebuffersAndPickers() {
    reflectFbo = new FrameBuffers();
    auto gui   = new GuiTexture(reflectFbo->getReflectionTexture(), glm::vec2(0.75f, 0.75f), glm::vec2(0.2f));
    guis.push_back(gui);

    picker = new TerrainPicker(playerCamera, renderer->getProjectionMatrix(), primaryTerrain);

    for (auto e : entities) {
        if (e->getBoundingBox() != nullptr) {
            allBoxes.push_back(e);
        }
    }
    allBoxes.reserve(entities.size() + scenes.size());
    allBoxes.insert(allBoxes.end(), entities.begin(), entities.end());
    allBoxes.insert(allBoxes.end(), scenes.begin(), scenes.end());
}

void Engine::run() {
    // All game logic uses delta-time via DisplayManager::getFrameTimeSeconds()
    while (DisplayManager::stayOpen()) {
        processFrame();
        DisplayManager::updateDisplay();
    }
}

void Engine::processFrame() {
    playerCamera->move(primaryTerrain);
    picker->update();

    handleObjectPicking();

    sampleModifiedGui->getPosition() += glm::vec2(0.001f, 0.001f);

    // Render to reflection framebuffer
    reflectFbo->bindReflectionFrameBuffer();
    {
        renderer->renderBoundingBoxes(allBoxes);
    }
    reflectFbo->unbindCurrentFrameBuffer();

    // Main scene render
    {
        renderer->renderScene(entities, scenes, allTerrains, lights);
    }

    pNameText->getPosition() += glm::vec2(.1f);

    UiMaster::render();
    UiMaster::getMasterComponent()->getConstraints()->getPosition() += glm::vec2(0.001f, 0.0f);
    UiMaster::applyConstraints(masterContainer);
}

void Engine::handleObjectPicking() {
    if (InputMaster::hasPendingClick()) {
        if (InputMaster::mouseClicked(LeftClick)) {
            renderer->renderBoundingBoxes(allBoxes);
            Color clickColor = Picker::getColor();
            int element      = BoundingBoxIndex::getIndexByColor(clickColor);

            *clickColorText = GUIText(
                ColorName::toString(clickColor) + ", Element: " + std::to_string(element),
                0.5f, fontModel, noodleFont, glm::vec2(10.0f, 20.0f), clickColor,
                0.75f * static_cast<float>(DisplayManager::Width()), false);

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
            InputMaster::resetClick();
        }
    }
}

void Engine::shutdown() {
    reflectFbo->cleanUp();
    TextMaster::cleanUp();
    fontRenderer->cleanUp();
    guiRenderer->cleanUp();
    rectRenderer->cleanUp();
    renderer->cleanUp();
    loader->cleanUp();
    DisplayManager::closeDisplay();
}

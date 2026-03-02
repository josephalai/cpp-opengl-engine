// Legacy — see Engine class for the active entry point.
//
// Created by Joseph Alai on 7/6/21.
//

#include "MainGameLoop.h"
#include "../Util/FileSystem.h"
#include "../Util/Utils.h"
#include "../Util/LightUtil.h"
#include "../RenderEngine/DisplayManager.h"
#include "../RenderEngine/EntityRenderer.h"
#include "../RenderEngine/ObjLoader.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Guis/Texture/GuiRenderer.h"
#include "../Guis/Rect/RectRenderer.h"
#include "../Toolbox/MousePicker.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "../Guis/Text/FontRendering/FontRenderer.h"
#include "../Util/ColorName.h"
#include "../Guis/Text/FontRendering/TextMaster.h"
#include "../Toolbox/TerrainPicker.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../Interaction/InteractiveModel.h"
#include "../Util/CommonHeader.h"
#include <thread>
#include "../Guis/GuiComponent.h"
#include "../Guis/UiMaster.h"
#include "../Guis/Rect/GuiRect.h"

void MainGameLoop::main() {

    // Initialite Display
    DisplayManager::createDisplay();

    // Initialize VAO / VBO Loader
    auto loader = new Loader();

    /**
     * Font Configuration
     */
    // Initialize Texting
    TextMaster::init(loader);

    auto fontRenderer = new FontRenderer();
    FontType arial = TextMeshData::loadFont("arial", 48);
    FontType noodle = TextMeshData::loadFont("noodle", 48);

    auto texts = new std::vector<GUIText *>();
    FontModel *fonty = loader->loadFontVAO();
    GUIText *text1 = new GUIText(
            "This is sample text because I know what I am doing whether you like it or not so I am joe.This is sample text because I know what I am doing whether you like it or not so I am joe.",
            0.50f, fonty, &noodle, glm::vec2(25.0f, 225.0f), ColorName::Whitesmoke,
            0.50f * static_cast<float>(DisplayManager::Width()),
            false);
    texts->push_back(text1);
    GUIText *text2 = new GUIText("Joseph Alai MCMXII", 0.5f, fonty, &arial, glm::vec2(540.0f, 50.0f),
                                 ColorName::Cyan,
                                 0.75f * static_cast<float>(DisplayManager::Width()), false);
    GUIText *pNameText = text2;
    texts->push_back(pNameText);
    auto clickColorText = new GUIText("Color: ", 0.5f, fonty, &arial, glm::vec2(10.0f, 20.0f), ColorName::Green,
                                      0.75f * static_cast<float>(DisplayManager::Width()), false);
    texts->push_back(clickColorText);

    /**
     * Loader Textures and Models
     */
    ModelData lampData;
    BoundingBoxData lampBbData;

    ModelData fernData;
    BoundingBoxData fernBbData;

    ModelData grassData;
    BoundingBoxData grassBnData;

    ModelData stallData;
    BoundingBoxData stallBbData;

    ModelData treeData;
    BoundingBoxData treeBbData;

    ModelData fluffyTreeData;
    BoundingBoxData fluffyTreeBbData;

    /**
     * Load the models concurrently
     */
    auto f = [](ModelData *pModelData, BoundingBoxData *pBbData, const std::string &filename) {
        *pModelData = OBJLoader::loadObjModel(filename);
        *pBbData = OBJLoader::loadBoundingBox(*pModelData, ClickBoxTypes::BOX, BoundTypes::AABB);
    };

    std::vector<std::thread> vThreads;

    std::vector<const std::string> modelFiles = {"lamp", "fern", "grassModel", "Stall", "tree", "fluffy-tree"};
    std::vector<ModelData *> modelDatas = {&lampData, &fernData, &grassData, &stallData, &treeData, &fluffyTreeData};
    std::vector<BoundingBoxData *> bbDatas = {&lampBbData, &fernBbData, &grassBnData, &stallBbData, &treeBbData,
                                              &fluffyTreeBbData};

    for (int i = 0; i < modelDatas.size(); ++i) {
        std::thread thread(f, modelDatas[i], bbDatas[i], modelFiles[i]);
        vThreads.push_back(std::move(thread));
    }

    for (auto &&modelThread : vThreads) {
        (modelThread).join();
    }
    /**
     * End loading the models concurrently.
     */

    // load bb version 1
    RawBoundingBox *pLampBox = loader->loadToVAO(lampBbData);
    RawModel *pLampModelData = loader->loadToVAO(lampData);
    auto staticLamp = new TexturedModel(pLampModelData, new ModelTexture("lamp", PNG));


    // loading bb version 2
    RawBoundingBox *pFernBox = loader->loadToVAO(fernBbData); // load by previously loaded object
    auto staticFern = new TexturedModel(loader->loadToVAO(fernData), new ModelTexture("fern", PNG));
    staticFern->getModelTexture()->setNumberOfRows(2);

    RawBoundingBox *pGrassBox = loader->loadToVAO(grassBnData);
    auto grassTexture = new ModelTexture("grassTexture", PNG, true, true);
    auto staticGrass = new TexturedModel(loader->loadToVAO(grassData), grassTexture);

    const Material material = Material{
            .shininess = 2.0f,
            .reflectivity = 2.0f
    };

    RawBoundingBox *pStallBox = loader->loadToVAO(stallBbData);
    auto staticStall = new TexturedModel(loader->loadToVAO(stallData),
                                         new ModelTexture("stallTexture", PNG, material));

    RawBoundingBox *pTreeBox = loader->loadToVAO(treeBbData);
    auto staticTree = new TexturedModel(loader->loadToVAO(treeData),
                                        new ModelTexture("tree", PNG, material));


    RawBoundingBox *pFluffyTreeBox = loader->loadToVAO(fluffyTreeBbData);
    auto staticFluffyTree = new TexturedModel(loader->loadToVAO(fluffyTreeData),
                                              new ModelTexture("tree", PNG, material));


    /**
     * Entity holders to be rendered
     */
    std::vector<Terrain *> allTerrains;
    std::vector<Light *> lights;
    std::vector<Entity *> entities;
    std::vector<AssimpEntity *> scenes;
    std::vector<Interactive *> allBoxes;

    /**
     * Terrain Generation
     */
    auto backgroundTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/grass")->getId());
    auto rTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/dirt")->getId());
    auto gTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blueflowers")->getId());
    auto bTexture = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/brickroad")->getId());
    auto texturePack = new TerrainTexturePack(backgroundTexture, rTexture, gTexture, bTexture);
    auto blendMap = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blendMap")->getId());
    auto terrain = new Terrain(0, -1, loader, texturePack, blendMap, "heightMap");

    /**
     * Push back all terrains into the array
     */
    allTerrains.push_back(new Terrain(-1, -1, loader, texturePack, blendMap, "heightMap"));
    allTerrains.push_back(new Terrain(0, 0, loader, texturePack, blendMap, "heightMap"));
    allTerrains.push_back(new Terrain(-1, 0, loader, texturePack, blendMap, "heightMap"));
    allTerrains.push_back(terrain);

    /**
     * Assimp Data
     */
    auto *pBackpack = new AssimpMesh("Backpack/backpack");
    auto pBackpackBox = OBJLoader::loadBoundingBox(pBackpack, ClickBoxTypes::BOX, BoundTypes::AABB);
    auto pBackpackBoxs = loader->loadToVAO(pBackpackBox);
    scenes.push_back(new AssimpEntity(pBackpack, new BoundingBox(pBackpackBoxs, BoundingBoxIndex::genUniqueId()),
                                      generateRandomPosition(terrain, 3.0f), generateRandomRotation(),
                                      generateRandomScale(3.25, 10.50)));


    /**
     * Light Generation
     */
    auto d = LightUtil::AttenuationDistance(65);
    Lighting l = Lighting{glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0.5f, 0.5f, 0.5f), d.x, d.y, d.z};
    lights.push_back(new Light(glm::vec3(0.0, 1000., -7000.0f), glm::vec3(0.4f, 0.4f, 0.4f), {
            .ambient =  glm::vec3(0.2f, 0.2f, 0.2f),
            .diffuse =  glm::vec3(0.5f, 0.5f, 0.5f),
            .constant = Light::kDirectional,
    }));
    lights.push_back(new Light(glm::vec3(120.0f, terrain->getHeightOfTerrain(120, -50) + 10, -50.0f),
                               glm::vec3(0.0f, 0.0f, 2.0f), l));
    lights.push_back(new Light(glm::vec3(100.0f, terrain->getHeightOfTerrain(100, -50) + 10, -50.0f),
                               glm::vec3(2.0f, 0.0f, 0.0f), l));
    lights.push_back(new Light(glm::vec3(110.0f, terrain->getHeightOfTerrain(110, -20) + 10, -20.0f),
                               glm::vec3(0.0, 2.0, 0.0f), l));



    /**
     * Load each entity into the vector, including position, fontSize, and rotation
     */
    auto lampy = new Entity(staticLamp, new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                            glm::vec3(120.0f, terrain->getHeightOfTerrain(120, -50), -50.0f));


    entities.push_back(lampy);
    entities.push_back(
            new Entity(staticStall, new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                       glm::vec3(1.0f, 0.0f, -82.4f),
                       glm::vec3(0.0f, 180.0f, 0.0f)));
    entities.push_back(new Entity(staticLamp, new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                                  glm::vec3(100.0f, terrain->getHeightOfTerrain(100, -50), -50.0f)));
    entities.push_back(new Entity(staticLamp, new BoundingBox(pLampBox, BoundingBoxIndex::genUniqueId()),
                                  glm::vec3(110.0f, terrain->getHeightOfTerrain(110, -20), -20.0f)));


    for (int i = 0; i < 500; ++i) {
        entities.push_back(
                new Entity(staticGrass, new BoundingBox(pGrassBox, BoundingBoxIndex::genUniqueId()),
                           generateRandomPosition(terrain),
                           generateRandomRotation(),
                           generateRandomScale(0.5, 1.50f)));
        entities.push_back(new Entity(staticFluffyTree, new BoundingBox(pFluffyTreeBox,
                                                                        BoundingBoxIndex::genUniqueId()),
                                      generateRandomPosition(terrain), generateRandomRotation(),
                                      generateRandomScale(0.5, 1.50f)));
        entities.push_back(
                new Entity(staticTree, new BoundingBox(pTreeBox, BoundingBoxIndex::genUniqueId()),
                           generateRandomPosition(terrain),
                           generateRandomRotation(),
                           generateRandomScale(.25, 1.50)));
        entities.push_back(
                new Entity(staticFern, new BoundingBox(pFernBox, BoundingBoxIndex::genUniqueId()), Utils::roll(1, 4),
                           generateRandomPosition(terrain), generateRandomRotation(),
                           generateRandomScale(.25, 1.50)));
    }

    /**
     * Player Creation
     */
    RawModel *playerModel = loader->loadToVAO(stallData);
    auto playerOne = new TexturedModel(playerModel, new ModelTexture(
            "stallTexture", PNG));

    auto player = new Player(playerOne, new BoundingBox(pStallBox, BoundingBoxIndex::genUniqueId()),
                             glm::vec3(100.0f, 3.0f, -50.0f),
                             glm::vec3(0.0f, 180.0f, 0.0f), 1.0f);
    InteractiveModel::setInteractiveBox(player);
    entities.push_back(player);
    auto playerCamera = new PlayerCamera(player);

    /**
     * GUI Creation
     */
    std::vector<GuiTexture *> guis = std::vector<GuiTexture *>();
    GuiTexture *t1 = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(), glm::vec2(-0.72f, 0.9f),
                                    glm::vec2(0.290f, 0.0900f));
    guis.push_back(t1);
    GuiTexture *t2 = new GuiTexture(loader->loadTexture("gui/green")->getId(), glm::vec2(-0.7f, 0.9f),
                                    glm::vec2(0.185f, 0.070f));
    guis.push_back(t2);
    GuiTexture *t3 = new GuiTexture(loader->loadTexture("gui/heart")->getId(), glm::vec2(-0.9f, 0.9f),
                                    glm::vec2(0.075f, 0.075f));
    guis.push_back(t3);
    auto sampleModifiedGui = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(), glm::vec2(-0.72f, 0.3f),
                                            glm::vec2(0.290f, 0.0900f) / 3.0f);
    guis.push_back(sampleModifiedGui);

    std::vector<GuiRect *> rects = std::vector<GuiRect *>();
    glm::vec3 color = glm::vec3(ColorName::Cyan.getR(), ColorName::Cyan.getG(), ColorName::Cyan.getB());
    glm::vec2 position = glm::vec2(-0.75f, 0.67f);
    glm::vec2 size = glm::vec2(0.290f, 0.0900f);
    glm::vec2 scale = glm::vec2(0.25f, 0.33f);
    float alpha = 0.33f;

    GuiRect *guiRect = new GuiRect(color, position, size, scale, alpha);
    glm::vec2 position2 = glm::vec2(-0.55f, 0.37f);
    glm::vec3 color2 = glm::vec3(ColorName::Green.getR(), ColorName::Green.getG(),
                                                    ColorName::Green.getB());
    GuiRect *guiRect2 = new GuiRect(color2, position2, size, scale, alpha);
    rects.push_back(guiRect);


    sampleModifiedGui->addChild(sampleModifiedGui, new UiConstraints(0, 0, 200, 200));


    /**
     * Renderers
     */
    auto renderer = new MasterRenderer(playerCamera, loader);
    auto guiRenderer = new GuiRenderer(loader);
    auto rectRenderer = new RectRenderer(loader);

    // initializes the UiMaster with these renderers.
    UiMaster::initialize(loader, guiRenderer, fontRenderer, rectRenderer);

    GuiComponent *masterContainer = UiMaster::getMasterComponent();

    GuiComponent *parent = new GuiComponent(Container::CONTAINER, new UiConstraints(0.01f, -0.01f, 50, 50));
    parent->setName("Parent");
    masterContainer->setName("Master Container");

    t1->setName("gui/lifebar");
    t2->setName("gui/green");
    t3->setName("gui/heart");



    masterContainer->addChild(guiRect, new UiConstraints(0.0f, -0.1f, 50, 50));
    masterContainer->addChild(guiRect2, new UiConstraints(0.0f, -0.1f, 50, 50));
    masterContainer->addChild(parent, new UiConstraints(0.02f, -0.1f, 50, 50));
    parent->addChild(t1, new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(t2, new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(t3, new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(text1, new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(text2, new UiConstraints(0.00f, -0.1f, 50, 50));
    parent->addChild(clickColorText, new UiConstraints(0.00f, -0.1f, 50, 50));

    t1->addChild(pNameText, new UiConstraints(-500.00f, 40.1f, 50, 50));

//    masterContainer->addChild(t1, new UiConstraints(0.0f, -0.1f, 50, 50));
//    masterContainer->addChild(t2, new UiConstraints(0.0f, -0.1f, 50, 50));
//    masterContainer->addChild(t3, new UiConstraints(0.0f, -0.1f, 50, 50));

    guiRect->setName("GuiRect");
    guiRect2->setName("GuiRect2");
    masterContainer->initialize();

    UiMaster::createRenderQueue(masterContainer);
    UiMaster::applyConstraints(masterContainer);


    /**
     * Framebuffers
     */
    auto reflectFbo = new FrameBuffers();
    auto gui = new GuiTexture(reflectFbo->getReflectionTexture(), glm::vec2(0.75f, 0.75f), glm::vec2(0.2f));
    guis.push_back(gui);

    /**
     * Mouse Picker
     */
    auto picker = new TerrainPicker(playerCamera, renderer->getProjectionMatrix(), terrain);

    for (auto e: entities) {
        if (e->getBoundingBox() != nullptr) {
            allBoxes.push_back(e);
        }
    }
    allBoxes.reserve(entities.size() + scenes.size());
    allBoxes.insert(allBoxes.end(), entities.begin(), entities.end());
    allBoxes.insert(allBoxes.end(), scenes.begin(), scenes.end());

    /**
     * Main Game Loop
     */
    while (DisplayManager::stayOpen()) {
        playerCamera->move(terrain);

        picker->update();

        glm::vec3 terrainPoint = picker->getCurrentTerrainPoint();

//        if (terrainPoint != glm::size()) {
//            lampy->setPosition(terrainPoint);
//            lights[1]->setPosition(glm::size(terrainPoint.x, terrainPoint.y + 15.0f, terrainPoint.z));
//        }

        /*
         * After the user has clicked, render the bounding boxes off-screen, and grab the pixel color of where their
         * mouse is. This color is the Hash ID generated which points to a specific object. So it returns the object.
         */
        if (InputMaster::hasPendingClick()) {
            if (InputMaster::mouseClicked(LeftClick)) {
                renderer->renderBoundingBoxes(allBoxes);
                Color clickColor = Picker::getColor();
                int element = BoundingBoxIndex::getIndexByColor(clickColor);
                *clickColorText = GUIText(ColorName::toString(clickColor) + ", Element: " + std::to_string(element),
                                          0.5f, fonty, &noodle, glm::vec2(10.0f, 20.0f), clickColor,
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
                    printf("position: x, y, z: (%f, %f, %f)\n", pClickedModel->getPosition().x,
                           pClickedModel->getPosition().y, pClickedModel->getPosition().z);
                }
                InputMaster::resetClick();
            }
        }

        sampleModifiedGui->getPosition() += glm::vec2(0.001f, 0.001f);

        // framebuffer only
        reflectFbo->bindReflectionFrameBuffer();
        {
            renderer->renderBoundingBoxes(allBoxes);
        }
        reflectFbo->unbindCurrentFrameBuffer();

        // non framebuffer
        {
            renderer->renderScene(entities, scenes, allTerrains, lights);
        }

        pNameText->getPosition() += glm::vec2(.1f);

//        TextMaster::render();
//        guiRenderer->render(guis);
//        rectRenderer->render(rects);
        UiMaster::render();
        UiMaster::getMasterComponent()->getConstraints()->getPosition() += glm::vec2(0.001f, 0.0f);
        UiMaster::applyConstraints(masterContainer);
        DisplayManager::updateDisplay();


    }
    /**
     * Clean up renderers and loaders
     */
    reflectFbo->cleanUp();
    TextMaster::cleanUp();
    fontRenderer->cleanUp();
    guiRenderer->cleanUp();
    rectRenderer->cleanUp();
    renderer->cleanUp();
    loader->cleanUp();

    /**
     * Close display
     */
    DisplayManager::closeDisplay();
}

glm::vec3 MainGameLoop::generateRandomPosition(Terrain *terrain, float yOffset) {
    glm::vec3 positionVector(0.0f);
    positionVector.x = floor(Utils::randomFloat() * 1500 - 800);
    positionVector.z = floor(Utils::randomFloat() * -800);
    positionVector.y = terrain->getHeightOfTerrain(positionVector.x, positionVector.z) + yOffset;
    return positionVector;
}

glm::vec3 MainGameLoop::generateRandomRotation() {
    float rx, ry, rz;
    rx = 0;
    ry = Utils::randomFloat() * 100 - 50;
    rz = 0;
    glm::vec3 rot(rx, ry, rz);
    rot = rot * 180.0f;
    return rot;
}

float MainGameLoop::generateRandomScale(float min = 0.75, float max = 1.50) {
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
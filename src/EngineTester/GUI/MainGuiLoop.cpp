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

// test to load objects.
#include <thread>

void MainGuiLoop::main() {

    // Initialite Display
    DisplayManager::createDisplay();

    // Initialize VAO / VBO Loader
    auto loader = new Loader();

    /**
     * Loader Textures and Models
     */
    ModelData stallData;
    BoundingBoxData stallBbData;

    /**
     * Load the models concurrently
     */
    auto f = [](ModelData *pModelData, BoundingBoxData *pBbData, const std::string &filename) {
        *pModelData = OBJLoader::loadObjModel(filename);
        *pBbData = OBJLoader::loadBoundingBox(*pModelData, ClickBoxTypes::BOX, BoundTypes::AABB);
    };

    std::vector<std::thread> vThreads;

    std::vector<const std::string> modelFiles = {"Stall"};
    std::vector<ModelData *> modelDatas = {&stallData};
    std::vector<BoundingBoxData *> bbDatas = {&stallBbData};

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

    const Material material = Material{
            .shininess = 2.0f,
            .reflectivity = 2.0f
    };

    RawBoundingBox *pStallBox = loader->loadToVAO(stallBbData);
    auto staticStall = new TexturedModel(loader->loadToVAO(stallData),
                                         new ModelTexture("stallTexture", PNG, material));

    /**
     * Entity holders to be rendered
     */
    std::vector<Terrain *> allTerrains;
    std::vector<Light *> lights;
    std::vector<Entity *> entities;
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
    allTerrains.push_back(terrain);

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
     * Renderers
     */
    auto fontRenderer = new FontRenderer();
    auto renderer = new MasterRenderer(playerCamera, loader);
    auto guiRenderer = new GuiRenderer(loader);
    auto rectRenderer = new RectRenderer(loader);


    /**
     * GUI Creation
     */
    // initializes the UiMaster with these renderers.
    UiMaster::initialize(loader, guiRenderer, fontRenderer, rectRenderer);

    std::vector<Container *> containers = std::vector<Container *>();

    std::vector<GuiTexture *> guis = std::vector<GuiTexture *>();


    TextureLoader *heartTexture = loader->loadTexture("gui/heart");
    auto *guiRedHeart = new GuiTexture(heartTexture->getId(), glm::vec2(-0.0f, 0.0f),
                                       glm::vec2(0.075f, 0.075f));
    auto *guiRedHeart2 = new GuiTexture(heartTexture->getId(), glm::vec2(-0.0f, 0.0f),
                                       glm::vec2(0.075f, 0.075f));
    guiRedHeart->setName("gui/heart");
    guiRedHeart2->setName("gui/heart2");

    // sample to see if we can move it.
    auto sampleModifiedGui = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(), glm::vec2(-0.72f, 0.3f),
                                            glm::vec2(0.290f, 0.0900f) / 3.0f);






    // background of inventoryParent slot (rect)
    Color color = ColorName::Whitesmoke;
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 size = glm::vec2(0.290f, 0.0900f);
    glm::vec2 scale = glm::vec2(0.25f, 0.25f);
    float alpha = 0.66f;
    // fixme: position is not accounted for
    auto *guiRect = new GuiRect(ColorName::Whitesmoke, position, size, scale, alpha);
    guiRect->setName("guiRect");

    glm::vec2 position2 = glm::vec2(-0.85f, 0.37f);
    glm::vec3 color2 = glm::vec3(ColorName::Green.getR(), ColorName::Green.getG(), ColorName::Green.getB());


    /**
    * Font Configuration
    */

    // Initialize Texting
    TextMaster::init(loader);

    FontType arialFont = TextMeshData::loadFont(FontNames::ARIAL, 48);
    FontType noodleFont = TextMeshData::loadFont(FontNames::NOODLE, 48);

    FontModel *fontLoaded = loader->loadFontVAO();
    // fixme: position is not accounted for
    auto *text1 = new GUIText(
            "This is a sample of a mutliline text. This is a sample of a mutliline text. This is a sample of a mutliline text. This is a sample of a mutliline text. This is a sample of a mutliline text.",
            0.50f, fontLoaded, &noodleFont, glm::vec2(0.0f, 0.0f), ColorName::Whitesmoke,
            0.50f * static_cast<float>(DisplayManager::Width()),
            false);
    text1->setName("multiline text");

    auto *text2 = new GUIText(
            "Inventory",
            0.30f, fontLoaded, &arialFont, glm::vec2(0.0f), ColorName::White, 0.50f * static_cast<float>(DisplayManager::Width()),
            false
            );
    text2->setName("Inventory");

    auto *inventoryCount = new GUIText(
            "217",
            0.23f, fontLoaded, &arialFont, glm::vec2(0.0f), ColorName::Yellow, 0.50f * static_cast<float>(DisplayManager::Width()),
            false
            );
    inventoryCount->setName("Count");

    // fixme: position is not accounted for

    GuiComponent *masterContainer = UiMaster::getMasterComponent();
    masterContainer->setName("Master Container");

    // simple container called inventoryParent.
    auto *inventoryParent = new GuiComponent(Container::CONTAINER, new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
                                                                                     new UiNormalizedConstraint(YAxis, 0.0f), 50,
                                                                                     50));
    // inventoryParent is the main container for the inventoryParent
    inventoryParent->setName("Inventory Parent");
    inventoryParent->setLayer(2);
    text1->setLayer(2);
    guiRedHeart->setLayer(2);
    guiRedHeart2->setLayer(2);
    text2->setLayer(3);
    inventoryCount->setLayer(3);
    guiRect->setLayer(1);
    masterContainer->addChild(inventoryParent, new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
                                                                 new UiNormalizedConstraint(YAxis, 0.0f), 50, 50));


    inventoryParent->addChild(guiRect, new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
                                                         new UiNormalizedConstraint(YAxis, 0.0f), 50, 50));

    inventoryParent->addChild(text1,
                              new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
                                                new UiNormalizedConstraint(YAxis, 0.4f), 50, 50));
    inventoryParent->addChild(guiRedHeart,
                              new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
                                                new UiNormalizedConstraint(YAxis, 0.0f), 50, 50));
    inventoryParent->addChild(guiRedHeart2,
                              new UiConstraints(new UiNormalizedConstraint(XAxis, 0.15f),
                                                new UiNormalizedConstraint(YAxis, 0.0f), 50, 50));
    inventoryParent->addChild(text2,
                              new UiConstraints(new UiNormalizedConstraint(XAxis, -0.075f),
                                                new UiNormalizedConstraint(YAxis, -0.04f), 50, 50));
    inventoryParent->addChild(inventoryCount,
                              new UiConstraints(new UiNormalizedConstraint(XAxis, -0.019f),
                                                new UiNormalizedConstraint(YAxis, 0.03f), 50, 50));

    guiRect->setName("GuiRect");

    masterContainer->initialize();

    UiMaster::applyConstraints();
    UiMaster::createRenderQueue();

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

    allBoxes.reserve(entities.size());
    allBoxes.insert(allBoxes.end(), entities.begin(), entities.end());

    /**
     * Main Game Loop
     */
    int sin = 0;
    while (DisplayManager::stayOpen()) {
        playerCamera->move(terrain);
        picker->update();

        /*
         * After the user has clicked, render the bounding boxes off-screen, and grab the pixel color of where their
         * mouse is. This color is the Hash ID generated which points to a specific object. So it returns the object.
         */
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
                    printf("position: x, y, z: (%f, %f, %f)\n", pClickedModel->getPosition().x,
                           pClickedModel->getPosition().y, pClickedModel->getPosition().z);
                }
                const string &renderString =
                        ColorName::toString(clickColor) + ", Element: " + std::to_string(element);
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
        renderer->renderScene(entities, std::vector<AssimpEntity *>(), allTerrains, lights);
//        text3->getPosition() += glm::vec2(.1f);

        // moves everything to the right
//        UiMaster::getMasterComponent()->constraints->position += glm::vec2(0.001f, 0.0f);
//        UiMaster::applyConstraints(masterContainer);

//        inventoryParent->constraints->position += glm::vec2(0.00f, 0.002f);
//        UiMaster::applyConstraints(inventoryParent);
//
//        guiLifeBarHolder->constraints->position += glm::vec2(0.00f, -0.001f);
//        UiMaster::applyConstraints(guiLifeBarHolder);
//
//        guiGreenBar->constraints->position += glm::vec2(0.00f, -0.005f);
//        guiGreenBar->addChild(text1, text1->getConstraints());
//        UiMaster::applyConstraints(guiRect);


        UiMaster::render();

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
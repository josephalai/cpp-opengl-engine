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
#include "../../Engine/SceneLoader.h"
#include "../../Scene/SceneGraph.h"
#include "../../Scene/SceneNode.h"
#include "../../Water/WaterTile.h"
#include "../../Water/WaterRenderer.h"
#include "../../Water/WaterShader.h"
#include "../../Atmosphere/FogSettings.h"
#include "../../Shadows/ShadowMap.h"
#include "../../Shaders/PBRMaterial.h"

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
    std::vector<WaterTile>      waterTiles;
    Terrain*                    primaryTerrain = nullptr;
    Player*                     player         = nullptr;
    PlayerCamera*               playerCamera   = nullptr;

    /**
     * Load scene from scene.cfg (falls back to minimal defaults if file missing)
     */
    bool sceneLoaded = SceneLoader::load(
        FileSystem::Scene("scene.cfg"),
        loader,
        entities, assimpEntities, lights, allTerrains, guis, waterTiles,
        primaryTerrain, player, playerCamera);

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

    std::vector<Container *> containers;

    TextureLoader *heartTexture = loader->loadTexture("gui/heart");
    auto *guiRedHeart  = new GuiTexture(heartTexture->getId(), glm::vec2(-0.0f, 0.0f), glm::vec2(0.075f, 0.075f));
    auto *guiRedHeart2 = new GuiTexture(heartTexture->getId(), glm::vec2(-0.0f, 0.0f), glm::vec2(0.075f, 0.075f));
    guiRedHeart->setName("gui/heart");
    guiRedHeart2->setName("gui/heart2");

    auto sampleModifiedGui = new GuiTexture(loader->loadTexture("gui/lifebar")->getId(),
                                            glm::vec2(-0.72f, 0.3f),
                                            glm::vec2(0.290f, 0.0900f) / 3.0f);

    Color     color    = ColorName::Whitesmoke;
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 size     = glm::vec2(0.290f, 0.0900f);
    glm::vec2 guiScale = glm::vec2(0.25f, 0.25f);
    float     alpha    = 0.66f;
    auto *guiRect = new GuiRect(ColorName::Whitesmoke, position, size, guiScale, alpha);
    guiRect->setName("guiRect");

    TextMaster::init(loader);
    FontType arialFont  = TextMeshData::loadFont(FontNames::ARIAL,  48);
    FontType noodleFont = TextMeshData::loadFont(FontNames::NOODLE, 48);
    FontModel *fontLoaded = loader->loadFontVAO();

    auto *text1 = new GUIText(
        "This is a sample of a mutliline text. This is a sample of a mutliline text. This is a sample of a mutliline text. This is a sample of a mutliline text. This is a sample of a mutliline text.",
        0.50f, fontLoaded, &noodleFont, glm::vec2(0.0f, 0.0f), ColorName::Whitesmoke,
        0.50f * static_cast<float>(DisplayManager::Width()), false);
    text1->setName("multiline text");

    auto *text2 = new GUIText("Inventory", 0.30f, fontLoaded, &arialFont,
        glm::vec2(0.0f), ColorName::White, 0.50f * static_cast<float>(DisplayManager::Width()), false);
    text2->setName("Inventory");

    auto *inventoryCount = new GUIText("217", 0.23f, fontLoaded, &arialFont,
        glm::vec2(0.0f), ColorName::Yellow, 0.50f * static_cast<float>(DisplayManager::Width()), false);
    inventoryCount->setName("Count");

    GuiComponent *masterContainer = UiMaster::getMasterComponent();
    masterContainer->setName("Master Container");

    auto *inventoryParent = new GuiComponent(
        Container::CONTAINER,
        new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
                          new UiNormalizedConstraint(YAxis, 0.0f), 50, 50));
    inventoryParent->setName("Inventory Parent");
    inventoryParent->setLayer(2);
    text1->setLayer(2);
    guiRedHeart->setLayer(2);
    guiRedHeart2->setLayer(2);
    text2->setLayer(3);
    inventoryCount->setLayer(3);
    guiRect->setLayer(1);

    masterContainer->addChild(inventoryParent,
        new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
                          new UiNormalizedConstraint(YAxis, 0.0f), 50, 50));
    inventoryParent->addChild(guiRect,
        new UiConstraints(new UiNormalizedConstraint(XAxis, 0.00f),
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
     * Main Game Loop
     * -----------------------------------------------------------------------
     */
    while (DisplayManager::stayOpen()) {
        playerCamera->move(pickerTerrain);
        picker->update();

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

        sampleModifiedGui->getPosition() += glm::vec2(0.001f, 0.001f);

        // --- Reflection pass for water (1.5) ---
        reflectFbo->bindReflectionFrameBuffer();
        renderer->renderBoundingBoxes(allBoxes);
        reflectFbo->unbindCurrentFrameBuffer();

        // --- Main scene pass via scene graph (1.2) ---
        renderer->renderSceneGraph(sceneGraph, assimpEntities, allTerrains, lights);

        // --- Water rendering after opaque geometry (1.5) ---
        renderer->renderWater(playerCamera, lights.empty() ? nullptr : lights[0]);

        UiMaster::render();

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

    DisplayManager::closeDisplay();
}

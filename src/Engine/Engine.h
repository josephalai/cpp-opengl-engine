#ifndef ENGINE_ENGINE_H
#define ENGINE_ENGINE_H

#include <vector>
#include <memory>
#include "../RenderEngine/MasterRenderer.h"
#include "../Guis/Texture/GuiRenderer.h"
#include "../Guis/Rect/RectRenderer.h"
#include "../Guis/Text/FontRendering/FontRenderer.h"
#include "../Guis/Text/FontRendering/TextMaster.h"
#include "../Guis/UiMaster.h"
#include "../Entities/Player.h"
#include "../Entities/PlayerCamera.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../Toolbox/TerrainPicker.h"
#include "../Interaction/InteractiveModel.h"
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"

class Engine {
public:
    Engine();
    ~Engine();

    /// Initialize the display, loader, and all subsystems
    void init();

    /// Run the main game loop
    void run();

    /// Clean up all resources and close display
    void shutdown();

private:
    /// Load fonts and text objects
    void initFonts();

    /// Load 3D models and textures (uses threading)
    void loadModels();

    /// Create terrain tiles
    void initTerrain();

    /// Spawn entities into the world
    void spawnEntities();

    /// Create player and camera
    void initPlayer();

    /// Set up lights
    void initLights();

    /// Initialize GUI components
    void initGui();

    /// Initialize renderers (MasterRenderer, GuiRenderer, etc.)
    void initRenderers();

    /// Initialize framebuffers and pickers
    void initFramebuffersAndPickers();

    /// Process a single frame (called inside the game loop)
    void processFrame();

    /// Handle click-based object picking
    void handleObjectPicking();

    // --- Subsystems (owned) ---
    Loader* loader = nullptr;
    MasterRenderer* renderer = nullptr;
    GuiRenderer* guiRenderer = nullptr;
    RectRenderer* rectRenderer = nullptr;
    FontRenderer* fontRenderer = nullptr;
    FrameBuffers* reflectFbo = nullptr;
    TerrainPicker* picker = nullptr;

    // --- Scene data ---
    Player* player = nullptr;
    PlayerCamera* playerCamera = nullptr;
    Terrain* primaryTerrain = nullptr;

    std::vector<Terrain*> allTerrains;
    std::vector<Light*> lights;
    std::vector<Entity*> entities;
    std::vector<AssimpEntity*> scenes;
    std::vector<Interactive*> allBoxes;
    std::vector<GuiTexture*> guis;
    std::vector<GuiRect*> rects;
    std::vector<GUIText*> texts;

    // Font data
    FontModel* fontModel = nullptr;
    GUIText* clickColorText = nullptr;
    GUIText* pNameText = nullptr;
    FontType* arialFont = nullptr;
    FontType* noodleFont = nullptr;

    // Models (loaded in loadModels, used in spawnEntities/initPlayer)
    TexturedModel* staticLamp = nullptr;
    TexturedModel* staticFern = nullptr;
    TexturedModel* staticGrass = nullptr;
    TexturedModel* staticStall = nullptr;
    TexturedModel* staticTree = nullptr;
    TexturedModel* staticFluffyTree = nullptr;
    RawBoundingBox* pLampBox = nullptr;
    RawBoundingBox* pFernBox = nullptr;
    RawBoundingBox* pGrassBox = nullptr;
    RawBoundingBox* pStallBox = nullptr;
    RawBoundingBox* pTreeBox = nullptr;
    RawBoundingBox* pFluffyTreeBox = nullptr;

    // GUI components updated per-frame
    GuiTexture* sampleModifiedGui = nullptr;
    GuiComponent* masterContainer = nullptr;
};

#endif // ENGINE_ENGINE_H

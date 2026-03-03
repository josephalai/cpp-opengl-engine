#ifndef ENGINE_ENGINE_H
#define ENGINE_ENGINE_H

#include <vector>
#include <memory>
#include "../RenderEngine/MasterRenderer.h"
#include "../Guis/Texture/Rendering/GuiRenderer.h"
#include "../Guis/Rect/Rendering/RectRenderer.h"
#include "../Guis/Text/Rendering/FontRenderer.h"
#include "../Guis/Text/Rendering/TextMaster.h"
#include "../Guis/UiMaster.h"
#include "../Entities/Player.h"
#include "../Entities/PlayerCamera.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../Toolbox/TerrainPicker.h"
#include "../Water/WaterRenderer.h"
#include "../Water/WaterShader.h"
#include "../Water/WaterTile.h"
#include "../Interaction/InteractiveModel.h"
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"
#include "../RenderEngine/AnimatedRenderer.h"
#include "../Shaders/AnimatedShader.h"

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

    /// Load 3-D scene from scene.cfg via SceneLoader; falls back to built-in
    /// defaults if the file is missing or cannot be parsed.
    void loadScene();

    /// Initialize GUI components (UI overlay, constraints hierarchy)
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
    WaterRenderer* waterRenderer = nullptr;
    WaterShader* waterShader = nullptr;

    // --- Scene data ---
    Player* player = nullptr;
    PlayerCamera* playerCamera = nullptr;
    Terrain* primaryTerrain = nullptr;

    std::vector<WaterTile> waterTiles;

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
    FontType* arialFont  = nullptr;
    FontType* noodleFont = nullptr;

    // GUI components updated per-frame
    GuiTexture* sampleModifiedGui = nullptr;
    GuiComponent* masterContainer = nullptr;

    // --- Animation ---
    AnimatedShader*   animShader   = nullptr;
    AnimatedRenderer* animRenderer = nullptr;
    std::vector<AnimatedEntity*> animatedEntities;
};

#endif // ENGINE_ENGINE_H

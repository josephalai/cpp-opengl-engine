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
#include "../RenderEngine/InstancedModel.h"
#include "../RenderEngine/InstancedModelManager.h"
#include "ISystem.h"
#include "EditorState.h"
#include "../Physics/PhysicsSystem.h"
#include <entt/entt.hpp>
#include <string>

class ChunkManager;
class NetworkSystem;
class PlayerMovementSystem;
class NetworkInterpolationSystem;
class OriginShiftSystem;

class Engine {
public:
    Engine();
    ~Engine();

    /// Initialize the display, loader, and all subsystems
    void init();

    /// Run the main game loop — thin coordinator that updates each ISystem in order
    void run();

    /// Clean up all resources and close display
    void shutdown();

    /// Central ECS registry — all entity/component data lives here.
    /// Public so that Systems can query and mutate components.
    entt::registry registry;

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

    /// Build the ordered list of ISystem instances that drive the game loop
    void buildSystems();

    /// Initialize Dear ImGui (call after DisplayManager::createDisplay())
    void initImGui();

    /// Shut down Dear ImGui
    void shutdownImGui();

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
    // Note: animated entities are managed via AnimatedModelComponent in the ECS
    // registry — no separate std::vector<AnimatedEntity*> needed.

    // --- Physics ---
    PhysicsSystem* physicsSystem = nullptr;

    // --- Instanced Rendering ---
    InstancedModelManager* instancedModelManager = nullptr;

    // --- Streaming ---
    ChunkManager* chunkManager = nullptr;
    std::string   terrainHeightmapFile = "heightMap"; ///< passed to ChunkManager

    // --- Origin Shift (Phase 4 Step 4.2) ---
    OriginShiftSystem* originShiftSystem_ = nullptr; ///< Non-owning ptr (owned by systems vec).

    // --- Network (Phase 5 Multi-Client) ---
    NetworkSystem* networkSystem_ = nullptr; ///< Non-owning ptr (owned by systems vec).
    std::string serverIP_ = "127.0.0.1"; ///< Read from ip.cfg or default.

    // --- Editor (GEA Step 5.3) ---
    EditorState editorState_;

    /// Read ip.cfg (if present) to set serverIP_.
    void loadIPConfig();

    /// Spawn callback for NetworkSystem — pure ECS entity spawn.
    /// Returns the entt::entity handle of the newly created entity.
    entt::entity onNetworkSpawn(uint32_t networkId, const std::string& modelType,
                                const glm::vec3& position);

    /// Despawn callback — destroys the ECS entity and releases its resources.
    void onNetworkDespawn(uint32_t networkId, entt::entity e);

    // --- ISystem ordered pipeline (owned) ---
    std::vector<std::unique_ptr<ISystem>> systems;
};

#endif // ENGINE_ENGINE_H

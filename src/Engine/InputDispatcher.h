// src/Engine/InputDispatcher.h
// An ISystem that runs at the top of the engine loop to translate raw
// InputMaster state into typed EventBus events.
//
// This is the single bridge between hardware input and gameplay logic.
// Systems that previously polled InputMaster::isKeyDown() for movement should
// instead subscribe to PlayerMoveCommandEvent via EventBus::instance().
// Systems that need the terrain right-click point should subscribe to
// TargetLocationClickedEvent instead of querying TerrainPicker directly.
//
// On a right-click, the dispatcher first asks EntityPicker whether the cursor
// is over an interactable entity (Ray-AABB test).  If so, it publishes an
// EntityClickedEvent so the NetworkSystem can send an ActionRequestPacket to
// the server.  If the ray misses all entities it falls back to the existing
// terrain-point behaviour (TargetLocationClickedEvent).
//
// Ordering constraint: InputDispatcher must be the first ISystem registered
// in Engine::buildSystems() so that event subscribers have up-to-date values
// before any other system's update() runs.

#ifndef ENGINE_INPUTDISPATCHER_H
#define ENGINE_INPUTDISPATCHER_H

#include "ISystem.h"
#include "EditorState.h"
#include <glm/glm.hpp>
#include <entt/entt.hpp>

class TerrainPicker;
class EntityPicker;
class Camera;

class InputDispatcher : public ISystem {
public:
    /// @param picker        Optional terrain picker used to resolve right-click
    ///                      terrain intersections for TargetLocationClickedEvent.
    /// @param editorState   Shared editor state; tilde press toggles editor mode.
    /// @param entityPicker  Optional Ray-AABB picker; when provided, right-click
    ///                      first tests entities before falling back to terrain.
    /// @param camera        Camera whose view matrix is used to build the pick ray.
    /// @param projection    Camera projection matrix (captured at construction time).
    /// @param registry      ECS registry used to look up NetworkIdComponent on the
    ///                      hit entity to populate EntityClickedEvent::networkId.
    explicit InputDispatcher(TerrainPicker*   picker       = nullptr,
                             EditorState*     editorState  = nullptr,
                             EntityPicker*    entityPicker = nullptr,
                             Camera*          camera       = nullptr,
                             glm::mat4        projection   = glm::mat4(1.0f),
                             entt::registry*  registry     = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    /// Shared entity-pick logic used by both left-click and right-click.
    /// Casts a ray from the current cursor position (or screen centre in FPS
    /// mode) and fires EntityClickedEvent if a NetworkIdComponent entity is hit.
    /// @return true if an entity was found and the event published.
    bool tryPickEntity();

    /// Phase 2 — right-click context menu.
    /// Casts a pick ray, finds the entity under the cursor, reads its available
    /// actions from the prefab JSON, and spawns an OSRS-style context menu at
    /// the cursor's screen coordinates.
    /// @return true if an entity was found and the context menu was shown.
    bool spawnContextMenu();

    TerrainPicker*  picker_;
    EditorState*    editorState_;
    EntityPicker*   entityPicker_;
    Camera*         camera_;
    glm::mat4       projection_;
    entt::registry* registry_;
    bool            prevRightClick_ = false; ///< edge-detect: fire once per right-click press
    bool            prevLeftClick_  = false; ///< edge-detect: fire once per left-click press
};

#endif // ENGINE_INPUTDISPATCHER_H

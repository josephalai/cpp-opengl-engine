#ifndef ECS_INPUTSTATECOMPONENT_H
#define ECS_INPUTSTATECOMPONENT_H

#include <glm/glm.hpp>
#include <vector>

class Terrain;
class PhysicsSystem;

/// Pure POD component — holds all per-entity input/movement state.
/// Replaces the old InputComponent (IComponent subclass) in the ECS migration.
/// A future PlayerMovementSystem will query registry.view<TransformComponent, InputStateComponent>()
/// each frame to apply movement logic.
///
/// [Phase 3.2] Network input flags mirror PlayerInputPacket exactly so the
/// local prediction path and the server authoritative path share the same data.
///
/// [Data-Driven] Default values for runSpeed, turnSpeed, kGravity, and
/// kJumpPower are now also defined in world_config.json and loaded by
/// ConfigManager.  EntityFactory applies config overrides when spawning
/// entities.  The static constexpr values below are retained as compile-time
/// fallbacks for code that runs before ConfigManager is initialised.
struct InputStateComponent {
    // --- Movement tuning (JSON-configurable via ConfigManager / prefabs) ---
    float runSpeed  = 20.0f;
    float turnSpeed = 160.0f;

    // --- Physics constants (compile-time fallbacks; runtime values come from ConfigManager) ---
    static constexpr float kGravity   = -50.0f;
    static constexpr float kJumpPower =  30.0f;

    // --- Per-frame state ---
    float currentSpeed     = 0.0f;
    float currentTurnSpeed = 0.0f;
    float upwardsSpeed     = 0.0f;
    bool  isInAir          = false;

    // --- Speed hack multiplier (shared across all instances) ---
    // NOTE: In the old InputComponent this was a static member. In the ECS world,
    // a global/singleton approach is cleaner than putting statics in a POD component.
    // For now we store it per-entity; the System can treat it as shared.
    float speedHack = 1.0f;

    // --- Network input flags (Phase 3.2) — mirrors PlayerInputPacket ---
    // These are polled by NetworkSystem each frame to build the outgoing packet.
    bool  moveForward  = false; ///< W key pressed this frame.
    bool  moveBackward = false; ///< S key pressed this frame.
    bool  moveLeft     = false; ///< A key pressed this frame.
    bool  moveRight    = false; ///< D key pressed this frame.
    bool  jump         = false; ///< Space key pressed this frame.
    float cameraYaw    = 0.0f;  ///< Absolute camera yaw (degrees) for movement direction.

    // --- Dependencies (non-owning pointers, set during init) ---
    Terrain*               terrain       = nullptr;
    std::vector<Terrain*>* allTerrains   = nullptr; ///< Live list of streamed tiles (Engine::allTerrains).
    PhysicsSystem*         physicsSystem = nullptr;

    // --- EventBus mode flag ---
    bool useEventBus = false;
};

#endif // ECS_INPUTSTATECOMPONENT_H

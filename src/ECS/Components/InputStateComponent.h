#ifndef ECS_INPUTSTATECOMPONENT_H
#define ECS_INPUTSTATECOMPONENT_H

#include <glm/glm.hpp>

class Terrain;
class PhysicsSystem;

/// Pure POD component — holds all per-entity input/movement state.
/// Replaces the old InputComponent (IComponent subclass) in the ECS migration.
/// A future PlayerMovementSystem will query registry.view<TransformComponent, InputStateComponent>()
/// each frame to apply movement logic.
struct InputStateComponent {
    // --- Movement tuning (JSON-configurable) ---
    float runSpeed  = 20.0f;
    float turnSpeed = 160.0f;

    // --- Physics constants ---
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

    // --- Dependencies (non-owning pointers, set during init) ---
    Terrain*       terrain       = nullptr;
    PhysicsSystem* physicsSystem = nullptr;

    // --- EventBus mode flag ---
    bool useEventBus = false;
};

#endif // ECS_INPUTSTATECOMPONENT_H

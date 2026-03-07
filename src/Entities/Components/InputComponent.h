//
// InputComponent.h — Pure-data keyboard-movement component.
//
// Phase 2 Step 3: update() has been removed.  The per-frame movement
// application logic now lives in InputSystem::update(), which reads the
// public data fields below and applies rotation / physics walk-direction
// each frame.  The EventBus subscription (subscribeToEvents()) remains here
// because it wires an internal applyMovementCommand() callback that only
// needs to update the component's data fields, not touch any entity transform.
//

#ifndef ENGINE_INPUTCOMPONENT_H
#define ENGINE_INPUTCOMPONENT_H

#include "IComponent.h"
#include "../../Input/InputMaster.h"
#include "../../Terrain/Terrain.h"
#include <nlohmann/json_fwd.hpp>

// Forward-declare to avoid pulling in the full Bullet / PhysicsSystem headers.
class PhysicsSystem;

class InputComponent : public IComponent {
public:
    // -------------------------------------------------------------------------
    // IComponent interface
    // -------------------------------------------------------------------------
    void init() override;

    /// JSON initialisation — load movement tuning from a prefab.
    /// Supported keys:
    ///   "run_speed"   (float) — units/sec when running
    ///   "turn_speed"  (float) — degrees/sec rotation rate
    void initFromJson(const nlohmann::json& j) override;

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /// Set the terrain used for height-collision in the legacy (non-physics) path.
    void setTerrain(Terrain* terrain) { terrain_ = terrain; }

    /// Wire to a live PhysicsSystem so Bullet handles gravity/collision.
    void setPhysicsSystem(PhysicsSystem* ps) { physicsSystem_ = ps; }

    /// After this call, direct InputMaster polling is skipped and the
    /// component relies on PlayerMoveCommandEvent from the EventBus.
    void subscribeToEvents();

    // -------------------------------------------------------------------------
    // Input polling (used by InputSystem when EventBus is not active)
    // -------------------------------------------------------------------------
    void checkInputs();

    // -------------------------------------------------------------------------
    // Movement constants
    // -------------------------------------------------------------------------
    static constexpr float kGravity   = -50.0f;
    static constexpr float kJumpPower = 30.0f;

    // -------------------------------------------------------------------------
    // Pure data — read/written by InputSystem each frame
    // -------------------------------------------------------------------------
    float currentSpeed_     = 0.0f;
    float currentTurnSpeed_ = 0.0f;
    float upwardsSpeed_     = 0.0f;
    bool  isInAir_          = false;

    // -------------------------------------------------------------------------
    // Dependencies (set once at init; read by InputSystem)
    // -------------------------------------------------------------------------
    Terrain*       terrain_       = nullptr;
    PhysicsSystem* physicsSystem_ = nullptr;
    bool           useEventBus_   = false;

private:
    // -------------------------------------------------------------------------
    // Movement tuning (JSON-configurable)
    // -------------------------------------------------------------------------
    static float SPEED_HACK;
    static constexpr float kDefaultRunSpeed  = 20.0f;
    static constexpr float kDefaultTurnSpeed = 160.0f;

    float runSpeed_  = kDefaultRunSpeed;
    float turnSpeed_ = kDefaultTurnSpeed;

    // -------------------------------------------------------------------------
    // Helpers — called internally by checkInputs() / EventBus handler
    // -------------------------------------------------------------------------
    void applyMovementCommand(float forward, float turn,
                              bool jump, bool sprint, bool sprintReset);
    void jump();
};

#endif // ENGINE_INPUTCOMPONENT_H

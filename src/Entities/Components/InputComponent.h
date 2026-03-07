//
// InputComponent.h — Keyboard-polling and character-movement component.
//
// Extracted from Player so that movement logic lives in a reusable
// IComponent rather than directly in the Entity subclass.
//

#ifndef ENGINE_INPUTCOMPONENT_H
#define ENGINE_INPUTCOMPONENT_H

#include "IComponent.h"
#include "../../Input/InputMaster.h"
#include "../../Terrain/Terrain.h"
#include <nlohmann/json.hpp>

// Forward-declare to avoid pulling in the full Bullet / PhysicsSystem headers.
class PhysicsSystem;

class InputComponent : public IComponent {
public:
    // -------------------------------------------------------------------------
    // IComponent interface
    // -------------------------------------------------------------------------
    void init() override;
    void update(float deltaTime) override;

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

private:
    // -------------------------------------------------------------------------
    // Movement constants (mirror of old Player constants)
    // -------------------------------------------------------------------------
    static float SPEED_HACK;
    static constexpr float kDefaultRunSpeed  = 20.0f;
    static constexpr float kDefaultTurnSpeed = 160.0f;
    static constexpr float kRunSpeed   = kDefaultRunSpeed;
    static constexpr float kTurnSpeed  = kDefaultTurnSpeed;
    static constexpr float kGravity    = -50.0f;
    static constexpr float kJumpPower  = 30.0f;

    // Per-instance overrides (JSON-configurable; initialised from static defaults)
    float runSpeed_  = kDefaultRunSpeed;
    float turnSpeed_ = kDefaultTurnSpeed;

    // -------------------------------------------------------------------------
    // Per-frame state
    // -------------------------------------------------------------------------
    float currentSpeed_     = 0.0f;
    float currentTurnSpeed_ = 0.0f;
    float upwardsSpeed_     = 0.0f;
    bool  isInAir_          = false;

    // -------------------------------------------------------------------------
    // Dependencies
    // -------------------------------------------------------------------------
    Terrain*       terrain_       = nullptr;
    PhysicsSystem* physicsSystem_ = nullptr;
    bool           useEventBus_   = false;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    void checkInputs();
    void applyMovementCommand(float forward, float turn,
                              bool jump, bool sprint, bool sprintReset);
    void jump();
};

#endif // ENGINE_INPUTCOMPONENT_H

//
// Created by Joseph Alai on 7/10/21.
//

#ifndef ENGINE_PLAYER_H
#define ENGINE_PLAYER_H
#include "Entity.h"
#include "../RenderEngine/DisplayManager.h"
#include "../Terrain/Terrain.h"

class PhysicsSystem;  ///< forward declaration — avoids circular include

class Player : public Entity {
private:
    static float SPEED_HACK;
    constexpr static const float kRunSpeed = 20;
    constexpr static const float kTurnSpeed = 160;
    constexpr static const float kGravity = -50;
    constexpr static const float kJumpPower = 30;

    constexpr static const float kTerrainHeight = 0;

    float currentSpeed = 0.0f;
    float currentTurnSpeed = 0.0f;
    float upwardsSpeed = 0;
    bool isInAir = false;

    /// When non-null, Player::move() delegates gravity and collision to Bullet.
    PhysicsSystem* physicsSystem_ = nullptr;

    void jump();

public:
    /**
     * @brief Player extends Entity: so it stores TexturedModel, as well as its' vectors
     *        to be able to manipulate the model. It also checks for input, and allows
     *        the user to move around, zoom in and out, etc.
     * @param model
     * @param position
     * @param rotation
     * @param scale
     */
    Player(TexturedModel *model, BoundingBox *box, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 rotation = glm::vec3(0),
            float scale = 1.0f) : Entity(model, box, position, rotation, scale){}

    void move(Terrain *terrain);

    /// Wire the player to a live PhysicsSystem so Bullet handles gravity and
    /// collision instead of the manual terrain-height fallback in move().
    void setPhysicsSystem(PhysicsSystem* ps) { physicsSystem_ = ps; }

    /// Subscribe to PlayerMoveCommandEvent on the global EventBus.
    /// After this call, checkInputs() skips direct InputMaster polling and
    /// relies on the event handler to keep currentSpeed / currentTurnSpeed
    /// up to date.  Call once during engine initialisation (after InputDispatcher
    /// has been registered as an ISystem).
    void subscribeToEvents();

private:
    bool useEventBus_ = false; ///< set by subscribeToEvents()

    /// Shared core of checkInputs() and the EventBus handler — applies
    /// normalised axis values and flags to the player's speed fields.
    void applyMovementCommand(float forward, float turn,
                              bool jump, bool sprint, bool sprintReset);

    void checkInputs();

};
#endif //ENGINE_PLAYER_H

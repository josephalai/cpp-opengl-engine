//
// CharacterController.h — physics-driven character controller that wraps the
// existing Player class using Bullet's btKinematicCharacterController.
//
// When active, Player::move() still reads keyboard input (checkInputs) and
// sets currentSpeed / currentTurnSpeed, but the actual position update is
// delegated to Bullet.  This replaces the hard-coded gravity and
// terrain-height collision in Player::move().
//

#ifndef ENGINE_CHARACTERCONTROLLER_H
#define ENGINE_CHARACTERCONTROLLER_H

#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

class Player;
class Terrain;
class PhysicsSystem;

class CharacterController {
public:
    CharacterController(PhysicsSystem* physicsSystem, Player* player,
                        float capsuleRadius = 0.5f, float capsuleHeight = 1.8f);
    ~CharacterController();

    /// Call each frame instead of Player::move(terrain).
    /// Reads player input and feeds walk direction + jump to Bullet.
    void update(float deltaTime, Terrain* terrain);

    btKinematicCharacterController* getBulletController() const {
        return controller_;
    }

private:
    PhysicsSystem*                  physicsSystem_ = nullptr;
    Player*                         player_        = nullptr;
    btKinematicCharacterController* controller_    = nullptr;  ///< owned by PhysicsSystem
};

#endif // ENGINE_CHARACTERCONTROLLER_H

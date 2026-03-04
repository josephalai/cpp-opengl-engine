#include "Player.h"
#include "../Input/InputMaster.h"
#include "../Physics/PhysicsSystem.h"
#include "../Events/Event.h"
#include "../Events/EventBus.h"

//
// Created by Joseph Alai on 7/10/21.
//
float Player::SPEED_HACK = 1.0f;

/**
 * @brief move (Main Loop), takes the terrain and moves the character based on the
 *        inputs. It MOVES / ROTATES it on the current terrain. It's utilizing Entity's
 *        information, as well as the DisplayManager to move around.
 * @param terrain
 */
void Player::move(Terrain *terrain) {
    checkInputs();
    rotate(glm::vec3(0.0f, currentTurnSpeed * DisplayManager::getFrameTimeSeconds(), 0.0f));

    float sinY = sin(glm::radians(getRotation().y));
    float cosY = cos(glm::radians(getRotation().y));

    if (physicsSystem_) {
        // Physics path: feed per-frame displacement to btKinematicCharacterController.
        // btKinematicCharacterController::setWalkDirection() (Bullet 3.x) takes a
        // displacement vector applied directly each physics tick — it does NOT
        // multiply internally by dt.  Scale by frame time here so movement speed
        // matches the legacy path (distance = currentSpeed * dt).
        // Bullet handles gravity, jumping, and collision response.
        float frameDt = DisplayManager::getFrameTimeSeconds();
        physicsSystem_->setPlayerWalkDirection(
            currentSpeed * sinY * frameDt,
            currentSpeed * cosY * frameDt,
            InputMaster::isKeyDown(Space));
        // Position sync (ghost → player) happens in PhysicsSystem::update()
        return;
    }

    // Legacy path: manual gravity + terrain-height collision (no Bullet).
    float distance = currentSpeed * DisplayManager::getFrameTimeSeconds();
    increasePosition(glm::vec3(distance * sinY, 0.0f, distance * cosY));
    upwardsSpeed += kGravity * DisplayManager::getFrameTimeSeconds();
    increasePosition(glm::vec3(0, upwardsSpeed * DisplayManager::getFrameTimeSeconds(), 0.0f));

    float terrainHeight = terrain->getHeightOfTerrain(getPosition().x, getPosition().z);
    if (getPosition().y <= terrainHeight) {
        upwardsSpeed = 0.0f;
        setPosition(+glm::vec3(getPosition().x, terrainHeight, getPosition().z));
        isInAir = false;
    }
}

void Player::jump() {
    if (!isInAir) {
        upwardsSpeed = kJumpPower;
        isInAir = true;
    }
}

void Player::subscribeToEvents() {
    useEventBus_ = true;
    // NOTE: the lambda captures `this`.  The Engine calls
    // EventBus::instance().clear() in Engine::shutdown() before any Player
    // is destroyed, so this pointer remains valid for the handler's lifetime.
    EventBus::instance().subscribe<PlayerMoveCommandEvent>(
        [this](const PlayerMoveCommandEvent& cmd) {
            applyMovementCommand(cmd.forward, cmd.turn,
                                 cmd.jump, cmd.sprint, cmd.sprintReset);
        });
}

void Player::applyMovementCommand(float forward, float turn,
                                  bool jump, bool sprint, bool sprintReset) {
    if      (forward > 0.0f) currentSpeed =  kRunSpeed * SPEED_HACK;
    else if (forward < 0.0f) currentSpeed = -kRunSpeed * SPEED_HACK;
    else                     currentSpeed = 0.0f;

    if      (turn > 0.0f) currentTurnSpeed =  kTurnSpeed * SPEED_HACK / 2;
    else if (turn < 0.0f) currentTurnSpeed = -kTurnSpeed * SPEED_HACK / 2;
    else                  currentTurnSpeed = 0.0f;

    if (sprint)      SPEED_HACK = 4.5f;
    if (sprintReset) SPEED_HACK = 1.0f;

    if (jump) this->jump();
}

void Player::checkInputs() {
    if (useEventBus_) {
        // Movement state is already applied by the PlayerMoveCommandEvent
        // handler registered in subscribeToEvents(). Skip direct polling.
        return;
    }

    // Legacy direct-poll path (active when subscribeToEvents() has not been
    // called, e.g. in unit tests or alternative entry points).
    float fwd  = InputMaster::isKeyDown(W) ?  1.0f
               : InputMaster::isKeyDown(S) ? -1.0f : 0.0f;
    float turn = InputMaster::isKeyDown(A) ?  1.0f
               : InputMaster::isKeyDown(D) ? -1.0f : 0.0f;
    applyMovementCommand(fwd, turn,
                         InputMaster::isKeyDown(Space),
                         InputMaster::isKeyDown(Tab),
                         InputMaster::isKeyDown(Backslash));
}
// src/Network/SharedMovement.cpp

#include "SharedMovement.h"
#include "../Config/ConfigManager.h"
#include <cmath>

// -------------------------------------------------------------------------
// Runtime accessors — read from ConfigManager, fall back to compiled defaults.
// -------------------------------------------------------------------------

float SharedMovement::runSpeed() {
    return ConfigManager::get().physics.defaultRunSpeed;
}

float SharedMovement::turnSpeed() {
    return ConfigManager::get().physics.defaultTurnSpeed;
}

float SharedMovement::gravity() {
    return ConfigManager::get().physics.gravity.y;
}

float SharedMovement::jumpPower() {
    return ConfigManager::get().physics.jumpPower;
}

void SharedMovement::applyInput(const Network::PlayerInputPacket& input,
                                glm::vec3& position, glm::vec3& rotation) {
    // --- Rotation — apply client's absolute camera yaw directly ---
    rotation.y = input.cameraYaw;

    // --- Forward / backward translation ---
    const float speed_val = runSpeed();
    float speed = 0.0f;
    if      (input.moveForward)  speed =  speed_val;
    else if (input.moveBackward) speed = -speed_val;

    float distance = speed * input.deltaTime;
    float sinY = std::sin(glm::radians(rotation.y));
    float cosY = std::cos(glm::radians(rotation.y));

    position.x += distance * sinY;
    position.z += distance * cosY;

    // --- Strafing (left / right) ---
    float strafeSpeed = 0.0f;
    if      (input.moveLeft)  strafeSpeed = -speed_val;
    else if (input.moveRight) strafeSpeed =  speed_val;

    float strafeDistance = strafeSpeed * input.deltaTime;
    position.x += strafeDistance *  cosY;
    position.z += strafeDistance * -sinY;

    // NOTE: vertical movement (gravity, jump, terrain landing) is intentionally
    // absent.  Bullet's btKinematicCharacterController drives the Y axis; the
    // server calls physicsSystem.jumpCharacterController(entity) when
    // input.jump is true, and Bullet's world gravity handles falling.
}

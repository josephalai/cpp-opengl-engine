// src/Network/SharedMovement.cpp

#include "SharedMovement.h"
#include <cmath>

void SharedMovement::applyInput(const Network::PlayerInputPacket& input,
                                glm::vec3& position, glm::vec3& rotation) {
    // --- Rotation — apply client's absolute camera yaw directly ---
    rotation.y = input.cameraYaw;

    // --- Forward / backward translation ---
    float speed = 0.0f;
    if      (input.moveForward)  speed =  kRunSpeed;
    else if (input.moveBackward) speed = -kRunSpeed;

    float distance = speed * input.deltaTime;
    float sinY = std::sin(glm::radians(rotation.y));
    float cosY = std::cos(glm::radians(rotation.y));

    position.x += distance * sinY;
    position.z += distance * cosY;

    // --- Strafing (left / right) ---
    float strafeSpeed = 0.0f;
    if      (input.moveLeft)  strafeSpeed = -kRunSpeed;
    else if (input.moveRight) strafeSpeed =  kRunSpeed;

    float strafeDistance = strafeSpeed * input.deltaTime;
    position.x += strafeDistance *  cosY;
    position.z += strafeDistance * -sinY;

    // NOTE: vertical movement (gravity, jump, terrain landing) is intentionally
    // absent.  Bullet's btKinematicCharacterController drives the Y axis; the
    // server calls physicsSystem.jumpCharacterController(entity) when
    // input.jump is true, and Bullet's world gravity handles falling.
}

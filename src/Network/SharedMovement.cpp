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
}

void SharedMovement::applyInput(const Network::PlayerInputPacket& input,
                                glm::vec3& position, glm::vec3& rotation,
                                float& upwardsSpeed, bool& isInAir,
                                float terrainHeight) {
    // --- XZ movement and rotation (delegates to the simple overload) ---
    applyInput(input, position, rotation);

    // --- Vertical physics ---

    // Jump: only when grounded and the jump key was pressed this input.
    if (input.jump && !isInAir) {
        upwardsSpeed = kJumpPower;
        isInAir      = true;
    }

    // Gravity accumulation and vertical displacement.
    upwardsSpeed += kGravity * input.deltaTime;
    position.y   += upwardsSpeed * input.deltaTime;

    // Terrain clamping — land when feet reach or pass through the surface.
    if (terrainHeight != kNoTerrainHeight && position.y <= terrainHeight) {
        upwardsSpeed = 0.0f;
        position.y   = terrainHeight;
        isInAir      = false;
    }
}

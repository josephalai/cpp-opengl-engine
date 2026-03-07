// src/Network/SharedMovement.cpp

#include "SharedMovement.h"
#include <cmath>

void SharedMovement::applyInput(const Network::PlayerInputPacket& input,
                                glm::vec3& position, glm::vec3& rotation,
                                float terrainHeight) {
    // --- Rotation — apply client's absolute camera yaw directly ---
    // [Phase 3.2] Replaced incremental turn (input.turn * kTurnSpeed) with
    // the authoritative cameraYaw sent by the client.
    rotation.y = input.cameraYaw;

    // --- Translation ---
    float speed = 0.0f;
    if      (input.moveForward)  speed =  kRunSpeed;
    else if (input.moveBackward) speed = -kRunSpeed;

    float distance = speed * input.deltaTime;
    float sinY = std::sin(glm::radians(rotation.y));
    float cosY = std::cos(glm::radians(rotation.y));

    position.x += distance * sinY;
    position.z += distance * cosY;

    // --- Strafing (moveLeft / moveRight) — perpendicular to facing direction ---
    float strafeSpeed = 0.0f;
    if      (input.moveLeft)  strafeSpeed = -kRunSpeed;
    else if (input.moveRight) strafeSpeed =  kRunSpeed;

    float strafeDistance = strafeSpeed * input.deltaTime;
    // Perpendicular to forward: rotate facing direction 90°.
    position.x += strafeDistance *  cosY;
    position.z += strafeDistance * -sinY;

    // --- Terrain height clamping ---
    if (terrainHeight > kNoTerrainHeight) {
        position.y = terrainHeight;
    }
}

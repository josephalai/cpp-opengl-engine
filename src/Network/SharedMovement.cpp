// src/Network/SharedMovement.cpp

#include "SharedMovement.h"
#include <cmath>

void SharedMovement::applyInput(const Network::PlayerInputPacket& input,
                                glm::vec3& position, glm::vec3& rotation,
                                float terrainHeight) {
    // --- Rotation ---
    float turnSpeed = 0.0f;
    if      (input.turn > 0.0f) turnSpeed =  kTurnSpeed / 2.0f;
    else if (input.turn < 0.0f) turnSpeed = -kTurnSpeed / 2.0f;
    rotation.y += turnSpeed * input.deltaTime;

    // --- Translation ---
    float speed = 0.0f;
    if      (input.forward > 0.0f) speed =  kRunSpeed;
    else if (input.forward < 0.0f) speed = -kRunSpeed;

    float distance = speed * input.deltaTime;
    float sinY = std::sin(glm::radians(rotation.y));
    float cosY = std::cos(glm::radians(rotation.y));

    position.x += distance * sinY;
    position.z += distance * cosY;

    // --- Terrain height clamping ---
    if (terrainHeight > kNoTerrainHeight) {
        position.y = terrainHeight;
    }
}

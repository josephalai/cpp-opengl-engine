// src/Network/SharedMovement.h
//
// Shared authoritative movement logic used by both the client (prediction /
// replay) and the headless server (authoritative simulation).
//
// By keeping the math in one place we guarantee that the client's predicted
// position matches the server's authoritative result, eliminating visual
// snapping under normal network conditions.

#ifndef ENGINE_SHARED_MOVEMENT_H
#define ENGINE_SHARED_MOVEMENT_H

#include "NetworkPackets.h"
#include <glm/glm.hpp>
#include <cmath>

namespace SharedMovement {

/// Movement constants — must match InputComponent for consistent prediction.
static constexpr float kRunSpeed  = 20.0f;
static constexpr float kTurnSpeed = 160.0f;

/// Apply a single input to position/rotation.
///
/// @param input         The player input to apply.
/// @param pos           [in/out] World-space position (modified in place).
/// @param rot           [in/out] Euler angles in degrees (modified in place).
/// @param terrainHeight Optional terrain height at the current XZ position.
///                      If provided, pos.y is clamped to this value after
///                      movement so the entity sticks to the terrain surface.
///                      Pass a negative sentinel (e.g. -99999) to skip.
inline void applyInput(const Network::PlayerInputPacket& input,
                       glm::vec3& pos, glm::vec3& rot,
                       float terrainHeight = -99999.0f) {
    // --- Rotation ---
    float turnSpeed = 0.0f;
    if      (input.turn > 0.0f) turnSpeed =  kTurnSpeed / 2.0f;
    else if (input.turn < 0.0f) turnSpeed = -kTurnSpeed / 2.0f;
    rot.y += turnSpeed * input.deltaTime;

    // --- Translation ---
    float speed = 0.0f;
    if      (input.forward > 0.0f) speed =  kRunSpeed;
    else if (input.forward < 0.0f) speed = -kRunSpeed;

    float distance = speed * input.deltaTime;
    float sinY = std::sin(glm::radians(rot.y));
    float cosY = std::cos(glm::radians(rot.y));

    pos.x += distance * sinY;
    pos.z += distance * cosY;

    // --- Terrain height clamping ---
    if (terrainHeight > -99998.0f) {
        pos.y = terrainHeight;
    }
}

} // namespace SharedMovement

#endif // ENGINE_SHARED_MOVEMENT_H

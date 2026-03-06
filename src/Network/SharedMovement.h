// src/Network/SharedMovement.h
//
// Shared authoritative movement logic used by both the client (prediction /
// replay) and the headless server (authoritative simulation).
//
// By keeping the math in one place we guarantee that the client's predicted
// position matches the server's authoritative result, eliminating visual
// snapping under normal network conditions.
//
// This is a pure mathematical utility with no rendering or networking
// dependencies — it can run identically on both the client and the headless
// server, ensuring deterministic simulation.

#ifndef ENGINE_SHARED_MOVEMENT_H
#define ENGINE_SHARED_MOVEMENT_H

#include "NetworkPackets.h"
#include <glm/glm.hpp>

class SharedMovement {
public:
    /// Movement constants — must match InputComponent for consistent prediction.
    static constexpr float kRunSpeed        = 20.0f;
    static constexpr float kTurnSpeed       = 160.0f;
    static constexpr float kNoTerrainHeight = -99999.0f;

    /// Apply a single input to position/rotation.
    ///
    /// @param input         The player input to apply.
    /// @param position      [in/out] World-space position (modified in place).
    /// @param rotation      [in/out] Euler angles in degrees (modified in place).
    /// @param terrainHeight Optional terrain height at the current XZ position.
    ///                      If provided (i.e. > kNoTerrainHeight), position.y is
    ///                      clamped to this value after movement.
    static void applyInput(const Network::PlayerInputPacket& input,
                           glm::vec3& position, glm::vec3& rotation,
                           float terrainHeight = kNoTerrainHeight);
};

#endif // ENGINE_SHARED_MOVEMENT_H

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
    /// Movement constants — must match InputStateComponent for consistent prediction.
    static constexpr float kRunSpeed        = 20.0f;
    static constexpr float kTurnSpeed       = 160.0f;
    static constexpr float kNoTerrainHeight = -99999.0f;

    /// Vertical physics constants — must match InputStateComponent exactly so the
    /// server's jump arc and the client's jump arc are bit-identical.
    static constexpr float kGravity   = -50.0f;
    static constexpr float kJumpPower =  30.0f;

    /// Apply a single input's horizontal movement and rotation.
    ///
    /// Vertical movement (gravity, jumping, terrain landing) is intentionally
    /// omitted here: Bullet's btKinematicCharacterController owns the Y axis
    /// and will apply world gravity + honour jump() calls autonomously.
    /// SharedMovement is the source of truth for XZ displacement only.
    ///
    /// @param input     The player input to apply.
    /// @param position  [in/out] World-space position (only X/Z modified).
    /// @param rotation  [in/out] Euler angles in degrees (Y modified).
    static void applyInput(const Network::PlayerInputPacket& input,
                           glm::vec3& position, glm::vec3& rotation);
};

#endif // ENGINE_SHARED_MOVEMENT_H

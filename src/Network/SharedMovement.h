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

    /// Apply a single input's horizontal movement and rotation (XZ only).
    ///
    /// This lightweight overload modifies only position.x / position.z and
    /// rotation.y.  The Y axis is intentionally untouched — use the full
    /// overload below when vertical physics are needed.
    ///
    /// @param input     The player input to apply.
    /// @param position  [in/out] World-space position (only XZ modified).
    /// @param rotation  [in/out] Euler angles in degrees (Y modified).
    static void applyInput(const Network::PlayerInputPacket& input,
                           glm::vec3& position, glm::vec3& rotation);

    /// Apply a single input's full movement: XZ translation, rotation, gravity,
    /// jumping, and terrain clamping.
    ///
    /// SharedMovement is the single authoritative source for the Y axis on both
    /// the server and the client.  Bullet Physics is used only for horizontal
    /// (XZ) wall-sliding; its gravity is disabled so it never fights the
    /// terrain-clamped heights this function produces.
    ///
    /// @param input         The player input to apply.
    /// @param position      [in/out] World-space position (XZ and Y modified).
    /// @param rotation      [in/out] Euler angles in degrees (Y modified).
    /// @param upwardsSpeed  [in/out] Vertical velocity (m/s). Persists across calls
    ///                      so gravity accumulates and jump arcs are smooth.
    /// @param isInAir       [in/out] Whether the entity is currently airborne.
    /// @param terrainHeight Terrain Y at the entity's XZ position, used for
    ///                      landing detection.  Pass kNoTerrainHeight to skip
    ///                      terrain clamping (entity will free-fall indefinitely).
    static void applyInput(const Network::PlayerInputPacket& input,
                           glm::vec3& position, glm::vec3& rotation,
                           float& upwardsSpeed, bool& isInAir,
                           float terrainHeight);
};

#endif // ENGINE_SHARED_MOVEMENT_H

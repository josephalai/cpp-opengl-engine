// src/Network/NetworkPackets.h
//
// Data Contracts (Packets) for the Authoritative Server / Dumb Client architecture.
//
// These plain-old-data structs are the strict memory layout shared between the
// game server and all connected clients.  Nothing here carries game logic —
// only raw state that can be serialised over a socket (Phase 2+).
//
// Phase 1 usage: the MockServer constructs TransformSnapshot values locally and
// pushes them into a NetworkSyncComponent's buffer, simulating packet arrival.

#ifndef ENGINE_NETWORK_PACKETS_H
#define ENGINE_NETWORK_PACKETS_H

#include <cstdint>
#include <glm/glm.hpp>

namespace Network {

/// A single authoritative snapshot of one entity's world transform.
/// The server stamps every outgoing packet with a monotonically increasing
/// sequenceNumber so the client can detect out-of-order or duplicate delivery.
/// timestamp is the server's simulation clock (seconds) at the moment this
/// snapshot was generated and is used by the client for smooth LERP playback.
struct TransformSnapshot {
    uint32_t  sequenceNumber = 0;     ///< Monotonically increasing packet counter.
    float     timestamp      = 0.0f;  ///< Server simulation time (seconds from start).
    glm::vec3 position       = {};    ///< World-space position.
    glm::vec3 rotation       = {};    ///< Euler angles in degrees, XYZ order
                                      ///< (pitch=X, yaw=Y, roll=Z), matching the
                                      ///< Entity::setRotation / Entity::getRotation API.
    uint32_t  lastProcessedInputSequence = 0; ///< Highest client input sequence the
                                              ///< server has processed; used by the
                                              ///< client for server reconciliation.
};

/// A single frame of player input sent from client to server.
/// The server applies the contained movement delta to the player's authoritative
/// state.  The client stores these in a history buffer for prediction replay.
struct PlayerInputPacket {
    uint32_t sequenceNumber = 0;  ///< Monotonically increasing per-client counter.
    float    forward        = 0.0f; ///< +1 = forward, -1 = backward, 0 = none.
    float    turn           = 0.0f; ///< +1 = turn left, -1 = turn right, 0 = none.
    float    deltaTime      = 0.0f; ///< Frame time used to scale the movement.
};

} // namespace Network

#endif // ENGINE_NETWORK_PACKETS_H

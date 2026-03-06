// src/Network/NetworkPackets.h
//
// Data Contracts (Packets) for the Authoritative Server / Dumb Client architecture.
//
// These plain-old-data structs are the strict memory layout shared between the
// game server and all connected clients.  Nothing here carries game logic —
// only raw state that can be serialised over a socket.
//
// Phase 5+: Every serialised message is prefixed with a single PacketType byte
// so the receiver can demultiplex different message kinds on the same channel.

#ifndef ENGINE_NETWORK_PACKETS_H
#define ENGINE_NETWORK_PACKETS_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <glm/glm.hpp>

namespace Network {

// -------------------------------------------------------------------------
// Packet type discriminator — first byte on the wire for every message
// -------------------------------------------------------------------------
enum class PacketType : uint8_t {
    TransformSnapshot = 1,
    PlayerInput       = 2,
    Welcome           = 3,
    Spawn             = 4,
    Despawn           = 5,
};

// -------------------------------------------------------------------------
// Packet structs (POD — safe to memcpy)
// -------------------------------------------------------------------------

/// A single authoritative snapshot of one entity's world transform.
struct TransformSnapshot {
    uint32_t  networkId     = 0;      ///< Entity this snapshot describes.
    uint32_t  sequenceNumber = 0;     ///< Monotonically increasing packet counter.
    float     timestamp      = 0.0f;  ///< Server simulation time (seconds from start).
    glm::vec3 position       = {};    ///< World-space position.
    glm::vec3 rotation       = {};    ///< Euler angles in degrees (pitch=X, yaw=Y, roll=Z).
    uint32_t  lastProcessedInputSequence = 0; ///< Highest client input sequence the
                                              ///< server has processed; used by the
                                              ///< client for server reconciliation.
};

/// A single frame of player input sent from client to server.
/// Includes the client's physics-driven position and rotation so the server
/// can use them as the authoritative state (with validation).
struct PlayerInputPacket {
    uint32_t  sequenceNumber = 0;    ///< Monotonically increasing per-client counter.
    float     forward        = 0.0f; ///< +1 = forward, -1 = backward, 0 = none.
    float     turn           = 0.0f; ///< +1 = turn left, -1 = turn right, 0 = none.
    float     deltaTime      = 0.0f; ///< Frame time used to scale the movement.
    glm::vec3 position       = {};   ///< Client's physics-driven world position.
    glm::vec3 rotation       = {};   ///< Client's physics-driven rotation (Euler degrees).
};

/// Sent by the server to a newly connected client to tell it its own networkId.
struct WelcomePacket {
    uint32_t localNetworkId = 0;
};

/// Sent by the server to tell clients about a new entity in the world.
static constexpr size_t kModelTypeLen = 32;
struct SpawnPacket {
    uint32_t  networkId = 0;
    char      modelType[kModelTypeLen] = {};
    glm::vec3 position  = {};
};

/// Sent by the server when an entity leaves the world.
struct DespawnPacket {
    uint32_t networkId = 0;
};

// -------------------------------------------------------------------------
// Wire-format helpers — prepend PacketType byte for sending / strip on recv
// -------------------------------------------------------------------------

/// Build a wire buffer: [ PacketType byte | raw struct bytes ].
template <typename T>
inline std::vector<uint8_t> serialise(PacketType type, const T& pkt) {
    std::vector<uint8_t> buf(1 + sizeof(T));
    buf[0] = static_cast<uint8_t>(type);
    std::memcpy(buf.data() + 1, &pkt, sizeof(T));
    return buf;
}

} // namespace Network

#endif // ENGINE_NETWORK_PACKETS_H

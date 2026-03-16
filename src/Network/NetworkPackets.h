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
    ActionRequest     = 6,  ///< Client requests a pathfound action on a target.
    ServerMessage     = 7,  ///< Server sends a text message to a specific client (e.g. NPC dialogue).
    ChatMessage       = 8,  ///< Phase 3 — Bidirectional spatial chat message.
    InventorySync     = 9,  ///< Phase 4 — Server pushes full inventory state to a client.
    InventoryMove     = 10, ///< Phase 4 — Client requests an item slot swap.
    SkillsSync        = 11, ///< Phase 5 — Server pushes full skills/XP state to a client.
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
/// [Phase 3.2] Protocol Shift: positional data (position, rotation) removed.
/// The client now sends only raw button states and the camera yaw; the server
/// independently simulates the resulting position via SharedMovement::applyInput().
/// This makes teleport-hacking physically impossible over the protocol.
struct PlayerInputPacket {
    uint32_t sequenceNumber = 0;     ///< Monotonically increasing per-client counter.
    float    deltaTime      = 0.0f;  ///< Frame time used to scale the movement.
    float    cameraYaw      = 0.0f;  ///< Absolute camera yaw (degrees) for movement direction.
    bool     moveForward    = false; ///< W key — move forward.
    bool     moveBackward   = false; ///< S key — move backward.
    bool     moveLeft       = false; ///< A key — strafe/turn left.
    bool     moveRight      = false; ///< D key — strafe/turn right.
    bool     jump           = false; ///< Space key — jump.
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

/// Phase 4 Step 4.4.2 — Client right-clicks a target and requests an action.
/// The server validates the request, runs an A* / NavMesh query, and assigns
/// a PathfindingComponent to auto-steer the player toward the target.
enum class ActionType : uint8_t {
    None    = 0,
    Attack  = 1,
    Harvest = 2,
    Talk    = 3,
    Use     = 4,
};

struct ActionRequestPacket {
    uint32_t   targetNetworkId = 0;     ///< Target entity to interact with.
    ActionType action          = ActionType::None;
};

/// Phase 6 — Server-to-client text message (NPC dialogue, notifications, etc.).
/// Sent reliably so the player always sees the message.
static constexpr size_t kMaxMessageLen = 128;
struct ServerMessagePacket {
    char message[kMaxMessageLen] = {};
};

// -------------------------------------------------------------------------
// Phase 3 — Spatial chat
// -------------------------------------------------------------------------

/// Fixed-length chat message buffer (client → server → nearby clients).
static constexpr size_t kMaxChatLen     = 128;
static constexpr size_t kMaxSenderLen   = 32;

/// Client sends this to the server; server redistributes to nearby players.
struct ChatMessagePacket {
    uint32_t senderNetworkId              = 0;         ///< Filled in by the server before relay.
    char     senderName[kMaxSenderLen]    = {};        ///< Display name resolved server-side.
    char     message[kMaxChatLen]         = {};        ///< UTF-8 message body (null-terminated).
};

// -------------------------------------------------------------------------
// Phase 4 — Inventory synchronisation
// -------------------------------------------------------------------------

static constexpr int kInventorySlots = 28; ///< Classic 28-slot OSRS inventory.

/// Server → Client: full snapshot of the player's inventory.
struct InventorySyncPacket {
    uint32_t itemIds[kInventorySlots]       = {}; ///< 0 = empty slot.
    uint32_t quantities[kInventorySlots]    = {}; ///< Stack count per slot.
};

/// Client → Server: request to move item from srcSlot to dstSlot.
struct InventoryMovePacket {
    uint8_t srcSlot = 0; ///< Source slot index [0, kInventorySlots).
    uint8_t dstSlot = 0; ///< Destination slot index [0, kInventorySlots).
};

// -------------------------------------------------------------------------
// Phase 5 — Skills synchronisation
// -------------------------------------------------------------------------

static constexpr int kSkillCount = 23; ///< Total number of tracked skills.

/// Server → Client: full snapshot of the player's skill XP values.
struct SkillsSyncPacket {
    uint32_t xp[kSkillCount] = {}; ///< Raw XP integer per skill (index = SkillId enum).
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

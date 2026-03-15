// src/Engine/NetworkSystem.h
//
// Phase 5+ — Multi-Client Networking with Unified Pipeline.
//
// ISystem that drives the client-side networking:
//   1. On init(), creates an ENet client host and connects to the server.
//   2. Handles Welcome / Spawn / Despawn / TransformSnapshot packets.
//   3. Maintains a map of networkId → entt::entity for remote interpolation.
//   4. Reads the actual Player's physics-driven position each frame and
//      sends it to the server — no separate prediction math needed.
//   5. For remote entities: pushes snapshots into their NetworkSyncData
//      buffers for smooth interpolation.

#ifndef ENGINE_NETWORKSYSTEM_H
#define ENGINE_NETWORKSYSTEM_H

#include "ISystem.h"
#include "../Entities/Entity.h"
#include "../ECS/Components/NetworkSyncData.h"
#include "../Network/NetworkPackets.h"
#include "../Input/InputMaster.h"
#include <vector>
#include <deque>
#include <string>
#include <unordered_map>
#include <functional>
#include <enet/enet.h>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

class PhysicsSystem;
class PlayerCamera;

class Player;

class NetworkSystem : public ISystem {
public:
    /// Callback the Engine provides so the NetworkSystem can dynamically
    /// create/destroy entities when Spawn/Despawn packets arrive.
    /// SpawnCallback returns an entt::entity handle (or entt::null on failure).
    using SpawnCallback   = std::function<entt::entity(uint32_t networkId,
                                                       const std::string& modelType,
                                                       const glm::vec3& position)>;
    using DespawnCallback = std::function<void(uint32_t networkId, entt::entity e)>;

    /// Construct with the registry (for ECS component access), the server IP,
    /// a pointer to the local Player, and optional entity callbacks.
    explicit NetworkSystem(entt::registry&  registry,
                           const std::string& serverIP,
                           Player*         localPlayer,
                           SpawnCallback   onSpawn   = nullptr,
                           DespawnCallback onDespawn = nullptr);

    void init()   override;
    void update(float deltaTime) override;
    void shutdown() override;

    /// Register an entity handle at runtime (called by SpawnCallback).
    void addEntity(uint32_t networkId, entt::entity e);

    /// Remove an entity by networkId (called by DespawnCallback).
    void removeEntity(uint32_t networkId);

    /// The local player's networkId assigned by the server.
    uint32_t localPlayerId() const { return localPlayerId_; }

    void setPhysicsSystem(PhysicsSystem* physics) { physicsSystem_ = physics; }

    /// Provide the PlayerCamera so that NetworkSystem can notify it when
    /// server-authoritative auto-walk begins or ends.  The camera
    /// automatically enters detached mode during auto-walk and restores
    /// the previous mode (with a smooth transition) when it ends.
    void setPlayerCamera(PlayerCamera* cam) { playerCamera_ = cam; }

    /// Send an ActionRequestPacket to the server, asking it to start an
    /// interaction between the local player and the target entity.
    ///
    /// @param targetNetworkId  The NetworkIdComponent::id of the clicked entity.
    void sendActionRequest(uint32_t targetNetworkId);

private:
    // ADD THIS STRUCT AND VECTOR:
    struct PlayerHistory {
        uint32_t sequenceNumber;
        glm::vec3 position;
    };
    std::vector<PlayerHistory> localHistory_;
    // --- Registry (for ECS component access) ---
    entt::registry& registry_;

    // --- Entity Map: networkId → ECS entity handle ---
    std::unordered_map<uint32_t, entt::entity> networkEntities_;

    // --- Local Player ---
    Player* localPlayer_ = nullptr;  ///< The actual physics-driven player entity.

    // --- Callbacks ---
    SpawnCallback   spawnCallback_;
    DespawnCallback despawnCallback_;

    // --- ENet state ---
    ENetHost* client_     = nullptr;
    ENetPeer* serverPeer_ = nullptr;

    std::string serverIP_;
    static constexpr int kServerPort    = 7777;
    static constexpr int kChannelCount  = 2;

    // --- Network state ---
    uint32_t  localPlayerId_       = 0;
    uint32_t  inputSequenceNumber_ = 0;

    // --- Local history buffer for server reconciliation ---
    static constexpr size_t kMaxLocalHistorySize = 100;

    // --- Server reconciliation ---
    // When the server disagrees with our position by more than kReconcileThresh²
    // we do NOT hard-snap the player (which causes visible teleporting). Instead
    // we store the server's authoritative position and smoothly LERP toward it
    // over the next several frames in update().
    static constexpr float kReconcileThreshSq = 0.1f; ///< squared-distance threshold
    static constexpr float kReconcileLerp     = 0.3f; ///< alpha per frame toward server
    glm::vec3 reconcileTarget_    = {};
    float     reconcileTargetYaw_ = 0.0f;
    bool      hasReconcileTarget_ = false;
    /// Countdown in seconds after the local player's spawn position is applied.
    /// While > 0 history-based reconciliation is skipped so the physics engine
    /// can settle without triggering a spurious LERP walk on the very first
    /// few frames.
    float     startupGracePeriod_ = 0.0f;
    PhysicsSystem* physicsSystem_ = nullptr;
    PlayerCamera*  playerCamera_  = nullptr;
};

#endif // ENGINE_NETWORKSYSTEM_H

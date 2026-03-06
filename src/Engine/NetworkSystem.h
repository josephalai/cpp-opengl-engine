// src/Engine/NetworkSystem.h
//
// Phase 5+ — Multi-Client Networking with Entity Registry.
//
// ISystem that drives the client-side networking:
//   1. On init(), creates an ENet client host and connects to the server.
//   2. Handles Welcome / Spawn / Despawn / TransformSnapshot packets.
//   3. Maintains a map of networkId → Entity* for remote interpolation.
//   4. Performs client-side prediction and server reconciliation for the
//      local player, while remote entities are smoothly interpolated.

#ifndef ENGINE_NETWORKSYSTEM_H
#define ENGINE_NETWORKSYSTEM_H

#include "ISystem.h"
#include "../Entities/Entity.h"
#include "../Entities/Components/NetworkSyncComponent.h"
#include "../Network/NetworkPackets.h"
#include "../Input/InputMaster.h"
#include <vector>
#include <deque>
#include <string>
#include <unordered_map>
#include <functional>
#include <enet/enet.h>
#include <glm/glm.hpp>

class NetworkSystem : public ISystem {
public:
    /// Callback the Engine provides so the NetworkSystem can dynamically
    /// create/destroy entities when Spawn/Despawn packets arrive.
    using SpawnCallback   = std::function<Entity*(uint32_t networkId,
                                                  const std::string& modelType,
                                                  const glm::vec3& position)>;
    using DespawnCallback = std::function<void(uint32_t networkId, Entity* e)>;

    /// Construct with the server IP and optional entity callbacks.
    explicit NetworkSystem(const std::string& serverIP,
                           SpawnCallback   onSpawn   = nullptr,
                           DespawnCallback onDespawn = nullptr);

    void init()   override;
    void update(float deltaTime) override;
    void shutdown() override;

    /// Register an additional entity at runtime (called by SpawnCallback).
    void addEntity(uint32_t networkId, Entity* e);

    /// Remove an entity by networkId (called by DespawnCallback).
    void removeEntity(uint32_t networkId);

    /// The local player's networkId assigned by the server.
    uint32_t localPlayerId() const { return localPlayerId_; }

private:
    // --- Entity Map (Phase 7) ---
    std::unordered_map<uint32_t, Entity*> networkEntities_;

    // --- Callbacks ---
    SpawnCallback   spawnCallback_;
    DespawnCallback despawnCallback_;

    // --- ENet state ---
    ENetHost* client_     = nullptr;
    ENetPeer* serverPeer_ = nullptr;

    std::string serverIP_;
    static constexpr int kServerPort    = 7777;
    static constexpr int kChannelCount  = 2;

    // --- Client-Side Prediction state ---
    uint32_t  localPlayerId_       = 0;
    std::deque<Network::PlayerInputPacket> pendingInputs_;
    glm::vec3 predictedPosition_ = {};
    glm::vec3 predictedRotation_ = {};
    uint32_t  inputSequenceNumber_ = 0;
};

#endif // ENGINE_NETWORKSYSTEM_H

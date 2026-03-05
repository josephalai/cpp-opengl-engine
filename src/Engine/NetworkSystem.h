// src/Engine/NetworkSystem.h
//
// Phase 2 — ENet Transport Layer.
//
// ISystem that drives the client-side networking:
//   1. On init(), creates an ENet client host and connects to the headless
//      server at 127.0.0.1:7777.
//   2. Each frame, polls ENet for incoming packets.  When a TransformSnapshot
//      packet arrives, it is deserialized and pushed into the registered
//      NetworkSyncComponent's buffer via pushSnapshot().
//   3. On shutdown(), disconnects gracefully and destroys the ENet host.
//
// The interpolation logic inside NetworkSyncComponent is unchanged — we are
// only replacing *how* the data arrives (ENet instead of MockServer).

#ifndef ENGINE_NETWORKSYSTEM_H
#define ENGINE_NETWORKSYSTEM_H

#include "ISystem.h"
#include "../Entities/Entity.h"
#include "../Entities/Components/NetworkSyncComponent.h"
#include <vector>
#include <enet/enet.h>

class NetworkSystem : public ISystem {
public:
    /// Pass the initial set of network-driven entities at construction.
    explicit NetworkSystem(std::vector<Entity*> netEntities);

    void init()   override;
    void update(float deltaTime) override;
    void shutdown() override;

    /// Register an additional entity at runtime.
    void addEntity(Entity* e);

private:
    std::vector<Entity*> netEntities_;

    // --- ENet state ---
    ENetHost* client_     = nullptr;
    ENetPeer* serverPeer_ = nullptr;

    static constexpr const char* kServerHost = "127.0.0.1";
    static constexpr int         kServerPort = 7777;
    /// Number of ENet channels (channel 0 = unreliable movement data).
    static constexpr int         kChannelCount = 2;
};

#endif // ENGINE_NETWORKSYSTEM_H

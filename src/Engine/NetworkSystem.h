// src/Engine/NetworkSystem.h
//
// ISystem that drives the Phase 1 mock-server / client-side interpolation layer.
//
// Responsibilities each frame:
//   1. Advance MockServer so it fires 10 Hz ticks and fills snapshot buffers.
//   2. Tick all registered network-driven entities so their components consume
//      the snapshots and smoothly interpolate the entity's transform.
//
// Adding more network entities:
//   Call addEntity(entity*) before the main loop starts.  Each entity must
//   already have a NetworkSyncComponent attached and that component must be
//   registered with MockServer.

#ifndef ENGINE_NETWORKSYSTEM_H
#define ENGINE_NETWORKSYSTEM_H

#include "ISystem.h"
#include "../Entities/Entity.h"
#include <vector>

class NetworkSystem : public ISystem {
public:
    /// Pass the initial set of network-driven entities at construction.
    explicit NetworkSystem(std::vector<Entity*> netEntities);

    void init()   override {}
    void update(float deltaTime) override;
    void shutdown() override {}

    /// Register an additional entity at runtime.
    void addEntity(Entity* e);

private:
    std::vector<Entity*> netEntities_;
};

#endif // ENGINE_NETWORKSYSTEM_H

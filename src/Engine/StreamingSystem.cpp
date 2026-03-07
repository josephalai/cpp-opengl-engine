// src/Engine/StreamingSystem.cpp
//
// Phase 2 Step 3 — Mandate 4: Pure ECS streaming via ActiveChunkTag.
//
// Instead of overwriting side-vectors, this system queries ChunkManager for
// the currently loaded entity/scene/terrain sets and stamps ActiveChunkTag on
// the corresponding registry entities.  Entities that leave active chunks have
// the tag removed.  RenderSystem and AnimationSystem filter their view<>
// queries with ActiveChunkTag to cull invisible objects without side-vectors.

#include "StreamingSystem.h"
#include "../ECS/Components/EntityOwnerComponent.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/TerrainComponent.h"
#include "../ECS/Components/ActiveChunkTag.h"
#include "../Entities/Player.h"
#include "../Entities/Entity.h"
#include "../Terrain/Terrain.h"
#include <unordered_set>

StreamingSystem::StreamingSystem(ChunkManager*    chunkManager,
                                  Player*          player,
                                  entt::registry&  registry)
    : chunkManager_(chunkManager)
    , player_(player)
    , registry_(registry)
{}

void StreamingSystem::update(float /*deltaTime*/) {
    if (!chunkManager_ || !player_) return;

    // Drive chunk load/unload based on the player's current world position.
    chunkManager_->update(player_->getPosition());

    // Re-assign entities whose positions have changed to the correct chunk
    // (e.g. network entities moved by NetworkSyncComponent) so they remain
    // in the correct spatial bucket after crossing chunk boundaries.
    chunkManager_->refreshEntityPositions();

    // -----------------------------------------------------------------------
    // Build O(1)-lookup sets from ChunkManager's active lists.
    // -----------------------------------------------------------------------
    auto activeEntities = chunkManager_->getActiveEntities();
    auto activeScenes   = chunkManager_->getActiveAssimpEntities();   // entt::entity handles
    auto activeTerrains = chunkManager_->getActiveTerrains();

    const std::unordered_set<Entity*>      entitySet (activeEntities.begin(), activeEntities.end());
    const std::unordered_set<entt::entity> sceneSet  (activeScenes.begin(),   activeScenes.end());
    const std::unordered_set<Terrain*>     terrainSet(activeTerrains.begin(), activeTerrains.end());

    // -----------------------------------------------------------------------
    // Stamp or remove ActiveChunkTag — Entity owners
    // -----------------------------------------------------------------------
    auto eView = registry_.view<EntityOwnerComponent>();
    for (auto [e, eoc] : eView.each()) {
        if (entitySet.count(eoc.ptr)) {
            registry_.emplace_or_replace<ActiveChunkTag>(e);
        } else {
            registry_.remove<ActiveChunkTag>(e);
        }
    }

    // -----------------------------------------------------------------------
    // Stamp or remove ActiveChunkTag — Assimp scene objects
    // ChunkManager tracks them by entt::entity handle, so we can stamp/remove
    // directly without comparing legacy pointers.
    // -----------------------------------------------------------------------
    auto sView = registry_.view<AssimpModelComponent>();
    for (auto [e, am] : sView.each()) {
        if (sceneSet.count(e)) {
            registry_.emplace_or_replace<ActiveChunkTag>(e);
        } else {
            registry_.remove<ActiveChunkTag>(e);
        }
    }

    // -----------------------------------------------------------------------
    // Stamp or remove ActiveChunkTag — Terrain tiles
    // -----------------------------------------------------------------------
    auto tView = registry_.view<TerrainComponent>();
    for (auto [e, tc] : tView.each()) {
        if (terrainSet.count(tc.terrain)) {
            registry_.emplace_or_replace<ActiveChunkTag>(e);
        } else {
            registry_.remove<ActiveChunkTag>(e);
        }
    }
}

void StreamingSystem::shutdown() {
    if (chunkManager_) {
        chunkManager_->shutdown();
        delete chunkManager_;
        chunkManager_ = nullptr;
    }
}

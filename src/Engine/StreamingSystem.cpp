// src/Engine/StreamingSystem.cpp
//
// Phase 2 Step 3: entities_ and scenes_ refs removed.
//   StreamingSystem only updates the terrain list and drives ChunkManager.
//   RenderSystem calls chunkManager_->getActiveEntities() / getActiveAssimpEntities()
//   directly each frame to obtain the current visible subset.

#include "StreamingSystem.h"
#include "../Entities/Player.h"

StreamingSystem::StreamingSystem(ChunkManager*            chunkManager,
                                  Player*                  player,
                                  std::vector<Terrain*>&   allTerrains)
    : chunkManager_(chunkManager)
    , player_(player)
    , allTerrains_(allTerrains)
{}

void StreamingSystem::update(float /*deltaTime*/) {
    if (!chunkManager_ || !player_) return;

    chunkManager_->update(player_->getPosition());

    // Re-assign entities whose positions have changed to the correct chunk
    // (e.g. network entities moved by NetworkSyncComponent).  This prevents
    // entities from becoming invisible when they cross chunk boundaries.
    chunkManager_->refreshEntityPositions();

    // Refresh the terrain list with the currently loaded chunks so RenderSystem
    // has the up-to-date active terrain set.
    allTerrains_ = chunkManager_->getActiveTerrains();
}

void StreamingSystem::shutdown() {
    if (chunkManager_) {
        chunkManager_->shutdown();
        delete chunkManager_;
        chunkManager_ = nullptr;
    }
}

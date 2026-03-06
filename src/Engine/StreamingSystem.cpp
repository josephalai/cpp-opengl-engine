// src/Engine/StreamingSystem.cpp

#include "StreamingSystem.h"
#include "../Entities/Player.h"

StreamingSystem::StreamingSystem(ChunkManager*               chunkManager,
                                  Player*                     player,
                                  std::vector<Terrain*>&      allTerrains,
                                  std::vector<Entity*>&       entities,
                                  std::vector<AssimpEntity*>& scenes)
    : chunkManager_(chunkManager)
    , player_(player)
    , allTerrains_(allTerrains)
    , entities_(entities)
    , scenes_(scenes)
{}

void StreamingSystem::update(float /*deltaTime*/) {
    if (!chunkManager_ || !player_) return;

    chunkManager_->update(player_->getPosition());

    // Re-assign entities whose positions have changed to the correct chunk
    // (e.g. network entities moved by NetworkSyncComponent).  This prevents
    // entities from becoming invisible when they cross chunk boundaries.
    chunkManager_->refreshEntityPositions();

    // Refresh the engine's scene lists with the currently loaded chunks.
    allTerrains_ = chunkManager_->getActiveTerrains();
    entities_    = chunkManager_->getActiveEntities();
    scenes_      = chunkManager_->getActiveAssimpEntities();
}

void StreamingSystem::shutdown() {
    if (chunkManager_) {
        chunkManager_->shutdown();
        delete chunkManager_;
        chunkManager_ = nullptr;
    }
}

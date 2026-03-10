// src/Engine/StreamingSystem.cpp

#include "StreamingSystem.h"
#include "GLUploadQueue.h"
#include "../Entities/Player.h"

StreamingSystem::StreamingSystem(ChunkManager*               chunkManager,
                                  Player*                     player,
                                  std::vector<Terrain*>&      allTerrains,
                                  std::vector<Entity*>&       entities)
    : chunkManager_(chunkManager)
    , player_(player)
    , allTerrains_(allTerrains)
    , entities_(entities)
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

    // Phase 4 Step 4.2 — Drain pending GL upload tasks from the async
    // chunk loader.  Process up to 2 per frame to keep frame times stable.
    GLUploadQueue::instance().processAll(/*maxPerFrame=*/2);

    // Phase 4 Step 4.2.3 — Process deferred entity-creation jobs within
    // the per-frame time budget.
    if (!deferredJobs_.empty()) {
        auto start = std::chrono::steady_clock::now();
        while (!deferredJobs_.empty()) {
            deferredJobs_.front()();
            deferredJobs_.pop();

            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - start).count();
            if (elapsed >= timeBudgetSec_) break;
        }
    }
}

void StreamingSystem::pushDeferredJob(std::function<void()> job) {
    deferredJobs_.push(std::move(job));
}

void StreamingSystem::shutdown() {
    if (chunkManager_) {
        chunkManager_->shutdown();
        delete chunkManager_;
        chunkManager_ = nullptr;
    }
}

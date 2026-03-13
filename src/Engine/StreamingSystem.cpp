// src/Engine/StreamingSystem.cpp

#include "StreamingSystem.h"
#include "GLUploadQueue.h"
#include "../Entities/Player.h"

StreamingSystem::StreamingSystem(ChunkManager*               chunkManager,
                                  Player*                     player,
                                  std::vector<Terrain*>&      allTerrains)
    : chunkManager_(chunkManager)
    , player_(player)
    , allTerrains_(allTerrains)
{}

void StreamingSystem::update(float /*deltaTime*/) {
    if (!chunkManager_ || !player_) return;

    chunkManager_->update(player_->getPosition());

    // Phase 4 Step 4.2 — Drain pending GL upload tasks from the async
    // chunk loader.  Process up to 2 per frame to keep frame times stable.
    // IMPORTANT: drain BEFORE refreshing allTerrains so that chunks
    // finalized this frame (state → LOADED) are included in the list that
    // PlayerMovementSystem uses for terrain-height collision.
    GLUploadQueue::instance().processAll(/*maxPerFrame=*/2);

    // Refresh the engine's terrain list with the currently loaded chunks.
    // Must come AFTER processAll so newly finalized chunks are included.
    allTerrains_ = chunkManager_->getActiveTerrains();

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

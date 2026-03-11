// src/Engine/StreamingSystem.h
// ISystem that drives dynamic chunk streaming each frame.
// Replaces the engine's allTerrains vector with the active
// subset from ChunkManager so that RenderSystem sees only loaded chunks.
//
// Phase 4 Step 4.2.3 — ECS time-slicing: limits entity instantiation to a
// maximum budget per frame (default 5 ms) to avoid frame drops.
//
// Phase 5.4 — Entity* tracking removed; static entities are ECS-only
// (StaticModelComponent) and are always visible via registry view.

#ifndef ENGINE_STREAMINGSYSTEM_H
#define ENGINE_STREAMINGSYSTEM_H

#include "ISystem.h"
#include "../Streaming/ChunkManager.h"
#include <vector>
#include <queue>
#include <chrono>
#include <functional>
#include <glm/glm.hpp>

class Terrain;
class Player;

class StreamingSystem : public ISystem {
public:
    /// Takes ownership of the ChunkManager.
    /// allTerrains is the Engine's terrain list; this system replaces
    /// its contents each frame with the active chunks' terrain tiles.
    StreamingSystem(ChunkManager*               chunkManager,
                    Player*                     player,
                    std::vector<Terrain*>&      allTerrains);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override;

    /// Phase 4 Step 4.2.3 — Push a deferred entity-creation job.
    /// Jobs are executed batch-by-batch over multiple frames until the
    /// per-frame budget is exhausted.
    void pushDeferredJob(std::function<void()> job);

    /// Set the per-frame time budget for deferred jobs (seconds).
    void setTimeBudget(float seconds) { timeBudgetSec_ = seconds; }

private:
    ChunkManager*               chunkManager_;
    Player*                     player_;
    std::vector<Terrain*>&      allTerrains_;

    /// Phase 4 Step 4.2.3 — Deferred entity creation queue.
    std::queue<std::function<void()>> deferredJobs_;
    float timeBudgetSec_ = 0.005f;  ///< 5 ms default.
};

#endif // ENGINE_STREAMINGSYSTEM_H

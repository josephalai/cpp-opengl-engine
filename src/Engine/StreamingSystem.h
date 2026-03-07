// src/Engine/StreamingSystem.h
// ISystem that drives dynamic chunk streaming each frame.
//
// Phase 2 Step 3 — Mandate 4: Fix the Streaming System Paradigm.
//   Instead of overwriting side-vectors every frame, StreamingSystem queries
//   ChunkManager for the current active sets and then stamps or removes
//   ActiveChunkTag on the corresponding registry entities.
//
//   RenderSystem and AnimationSystem include ActiveChunkTag in their view<>
//   queries so they naturally cull inactive entities without needing arrays.

#ifndef ENGINE_STREAMINGSYSTEM_H
#define ENGINE_STREAMINGSYSTEM_H

#include "ISystem.h"
#include "../Streaming/ChunkManager.h"
#include <entt/entt.hpp>

class Player;

class StreamingSystem : public ISystem {
public:
    /// @param chunkManager  Takes ownership of the ChunkManager.
    /// @param registry      Engine-level ECS registry; StreamingSystem stamps
    ///                      ActiveChunkTag to control which entities are rendered.
    StreamingSystem(ChunkManager*    chunkManager,
                    Player*          player,
                    entt::registry&  registry);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override;

private:
    ChunkManager*    chunkManager_;
    Player*          player_;
    entt::registry&  registry_;
};

#endif // ENGINE_STREAMINGSYSTEM_H

// src/Engine/StreamingSystem.h
// ISystem that drives dynamic chunk streaming each frame.
// Replaces the engine's allTerrains/entities/scenes vectors with the active
// subset from ChunkManager so that RenderSystem sees only loaded chunks.

#ifndef ENGINE_STREAMINGSYSTEM_H
#define ENGINE_STREAMINGSYSTEM_H

#include "ISystem.h"
#include "../Streaming/ChunkManager.h"
#include <vector>
#include <glm/glm.hpp>

class Entity;
class AssimpEntity;
class Terrain;
class Player;

class StreamingSystem : public ISystem {
public:
    /// Takes ownership of the ChunkManager.
    /// The three vectors are the Engine's scene lists; this system replaces
    /// their contents each frame with the active chunks' data.
    StreamingSystem(ChunkManager*               chunkManager,
                    Player*                     player,
                    std::vector<Terrain*>&      allTerrains,
                    std::vector<Entity*>&       entities,
                    std::vector<AssimpEntity*>& scenes);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override;

private:
    ChunkManager*               chunkManager_;
    Player*                     player_;
    std::vector<Terrain*>&      allTerrains_;
    std::vector<Entity*>&       entities_;
    std::vector<AssimpEntity*>& scenes_;

    /// Frame counter and last-seen entity count for [NetTrace] log throttling.
    uint32_t    logFrameCounter_ = 0;
    std::size_t lastEntityCount_ = static_cast<std::size_t>(-1); ///< sentinel
    static constexpr uint32_t kLogInterval = 60; ///< log at most once per 60 frames
};

#endif // ENGINE_STREAMINGSYSTEM_H

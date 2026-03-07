// src/Engine/StreamingSystem.h
// ISystem that drives dynamic chunk streaming each frame.
// Replaces the engine's allTerrains/entities vectors with the active
// subset from ChunkManager so that RenderSystem sees only loaded chunks.

#ifndef ENGINE_STREAMINGSYSTEM_H
#define ENGINE_STREAMINGSYSTEM_H

#include "ISystem.h"
#include "../Streaming/ChunkManager.h"
#include <vector>
#include <glm/glm.hpp>

class Entity;
class Terrain;
class Player;

class StreamingSystem : public ISystem {
public:
    /// Takes ownership of the ChunkManager.
    /// The two vectors are the Engine's scene lists; this system replaces
    /// their contents each frame with the active chunks' data.
    StreamingSystem(ChunkManager*               chunkManager,
                    Player*                     player,
                    std::vector<Terrain*>&      allTerrains,
                    std::vector<Entity*>&       entities);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override;

private:
    ChunkManager*               chunkManager_;
    Player*                     player_;
    std::vector<Terrain*>&      allTerrains_;
    std::vector<Entity*>&       entities_;
};

#endif // ENGINE_STREAMINGSYSTEM_H

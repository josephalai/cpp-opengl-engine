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
};

#endif // ENGINE_STREAMINGSYSTEM_H

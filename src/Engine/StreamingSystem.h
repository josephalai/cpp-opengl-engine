// src/Engine/StreamingSystem.h
// ISystem that drives dynamic chunk streaming each frame.
// Updates allTerrains with the active subset from ChunkManager so that
// RenderSystem sees only loaded chunks.
//
// Phase 2 Step 3: entities and scenes vector refs are removed.  RenderSystem
// queries ChunkManager directly for active Entity*/AssimpEntity* subsets.

#ifndef ENGINE_STREAMINGSYSTEM_H
#define ENGINE_STREAMINGSYSTEM_H

#include "ISystem.h"
#include "../Streaming/ChunkManager.h"
#include <vector>
#include <glm/glm.hpp>

class Terrain;
class Player;

class StreamingSystem : public ISystem {
public:
    /// @param chunkManager  Takes ownership of the ChunkManager.
    /// @param allTerrains   Engine's terrain list — updated with active chunks each frame.
    StreamingSystem(ChunkManager*            chunkManager,
                    Player*                  player,
                    std::vector<Terrain*>&   allTerrains);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override;

private:
    ChunkManager*           chunkManager_;
    Player*                 player_;
    std::vector<Terrain*>&  allTerrains_;
};

#endif // ENGINE_STREAMINGSYSTEM_H

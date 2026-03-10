// src/Streaming/StreamingChunk.h
// Represents one terrain tile and its associated entities at a grid position.

#ifndef ENGINE_STREAMINGCHUNK_H
#define ENGINE_STREAMINGCHUNK_H

#include <vector>
#include <string>
#include "../Terrain/Terrain.h"

class Entity;
class Loader;
class TerrainTexturePack;
class TerrainTexture;

class StreamingChunk {
public:
    enum class State { UNLOADED, LOADING, LOADED, UNLOADING };

    int   gridX = 0;
    int   gridZ = 0;
    State state = State::UNLOADED;

    Terrain*                    terrain  = nullptr;
    std::vector<Entity*>        entities;

    /// GEA Step 5.4 — Tracks whether baked entity spawns have been fired
    /// for this chunk.  Pre-registered terrain chunks (via registerTerrain)
    /// start with this as false so that ChunkManager::update() can fire
    /// their baked spawns on the first frame.
    bool bakedSpawned = false;

    StreamingChunk() = default;
    StreamingChunk(int gx, int gz) : gridX(gx), gridZ(gz) {}

    /// Create and load the terrain tile for this grid cell (synchronous).
    void load(Loader* loader,
              TerrainTexturePack* texPack,
              TerrainTexture*     blendMap,
              const std::string&  heightmapFile);

    /// Phase 4 Step 4.2 — Finalize an async load by constructing a Terrain
    /// from pre-parsed TerrainData on the GL thread.
    void finalizeAsync(const TerrainData& data, Loader* loader,
                       TerrainTexturePack* texPack, TerrainTexture* blendMap);

    /// Register an externally created Terrain (not owned by this chunk).
    /// The terrain will NOT be deleted when the chunk is unloaded.
    void setExternalTerrain(Terrain* t);

    void addEntity(Entity* e);

    /// Release all terrain and entity resources.
    void unload();

private:
    bool terrainOwned_ = true; ///< false → terrain was externally created
};

#endif // ENGINE_STREAMINGCHUNK_H

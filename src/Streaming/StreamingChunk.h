// src/Streaming/StreamingChunk.h
// Represents one terrain tile at a grid position.
// Entity tracking has been removed (Phase 5.4): static entities are
// stored as ECS StaticModelComponent entities in the registry, and
// network/animated entities are tracked via AnimatedModelComponent.

#ifndef ENGINE_STREAMINGCHUNK_H
#define ENGINE_STREAMINGCHUNK_H

#include <vector>
#include <string>
#include "../Terrain/Terrain.h"

class Loader;
class TerrainTexturePack;
class TerrainTexture;

class StreamingChunk {
public:
    enum class State { UNLOADED, LOADING, LOADED, UNLOADING };

    int   gridX = 0;
    int   gridZ = 0;
    State state = State::UNLOADED;

    Terrain* terrain  = nullptr;

    /// GEA Step 5.4 — Tracks whether baked entity spawns have been fired
    /// for this chunk.
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
    void setExternalTerrain(Terrain* t);

    /// Release all terrain resources.
    void unload();

private:
    bool terrainOwned_ = true;
};

#endif // ENGINE_STREAMINGCHUNK_H

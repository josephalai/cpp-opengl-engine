// src/Streaming/StreamingChunk.h
// Represents one terrain tile and its associated entities at a grid position.

#ifndef ENGINE_STREAMINGCHUNK_H
#define ENGINE_STREAMINGCHUNK_H

#include <vector>
#include <string>

class Terrain;
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

    StreamingChunk() = default;
    StreamingChunk(int gx, int gz) : gridX(gx), gridZ(gz) {}

    /// Create and load the terrain tile for this grid cell.
    void load(Loader* loader,
              TerrainTexturePack* texPack,
              TerrainTexture*     blendMap,
              const std::string&  heightmapFile);

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

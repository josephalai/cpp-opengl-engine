// src/Streaming/ChunkManager.h
// Manages a grid of StreamingChunks keyed by (gridX, gridZ).
// Chunks within loadRadius of the player are loaded; chunks beyond
// unloadRadius are unloaded.

#ifndef ENGINE_CHUNKMANAGER_H
#define ENGINE_CHUNKMANAGER_H

#include "StreamingChunk.h"
#include "../Terrain/Terrain.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Loader;
class TerrainTexturePack;
class TerrainTexture;
class Entity;
class AssimpEntity;

class ChunkManager {
public:
    /// Terrain tile size in world units — mirrors Terrain::kSize.
    static constexpr float kTerrainSize = Terrain::kSize;

    ChunkManager(Loader*             loader,
                 TerrainTexturePack* texPack,
                 TerrainTexture*     blendMap,
                 const std::string&  heightmapFile);

    ~ChunkManager();

    /// Update which chunks are loaded based on the player's world position.
    void update(const glm::vec3& playerPosition);

    /// Register an entity so it appears in the chunk that covers worldPos.
    void registerEntity(Entity* e, const glm::vec3& worldPos);

    /// Remove an entity from whichever chunk currently holds it.
    void removeEntity(Entity* e);

    /// Register an AssimpEntity in the appropriate chunk.
    void registerAssimpEntity(AssimpEntity* e, const glm::vec3& worldPos);

    /// Register a pre-existing (SceneLoader-owned) terrain tile so the
    /// ChunkManager knows its grid cell and won't create a duplicate.
    /// The terrain is NOT deleted when the chunk is unloaded.
    void registerTerrain(Terrain* t);

    /// Re-assign entities to the correct chunk based on their current
    /// world-space position.  Call once per frame (from StreamingSystem)
    /// so that entities whose positions are updated externally (e.g.
    /// NetworkSyncComponent) remain in the correct spatial bucket.
    void refreshEntityPositions();

    /// Collect all Terrain pointers from LOADED chunks.
    std::vector<Terrain*>      getActiveTerrains()      const;
    /// Collect all Entity pointers from LOADED chunks.
    std::vector<Entity*>       getActiveEntities()       const;
    /// Collect all AssimpEntity pointers from LOADED chunks.
    std::vector<AssimpEntity*> getActiveAssimpEntities() const;

    /// Unload all chunks and release resources.
    void shutdown();

private:
    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const {
            return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 16);
        }
    };

    std::unordered_map<std::pair<int,int>, StreamingChunk*, PairHash> chunks_;

    Loader*             loader_;
    TerrainTexturePack* texPack_;
    TerrainTexture*     blendMap_;
    std::string         heightmapFile_;

    int loadRadius_   = 2;
    int unloadRadius_ = 3;
};

#endif // ENGINE_CHUNKMANAGER_H

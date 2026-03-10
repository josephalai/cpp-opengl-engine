// src/Streaming/ChunkManager.h
// Manages a grid of StreamingChunks keyed by (gridX, gridZ).
// Chunks within loadRadius of the player are loaded; chunks beyond
// unloadRadius are unloaded.
//
// Phase 4 Step 4.2.1 — Background thread pool for async chunk loading.
// Step 4.2.2 — Three streaming radii: Active, Loading, Unloading.

#ifndef ENGINE_CHUNKMANAGER_H
#define ENGINE_CHUNKMANAGER_H

#include "StreamingChunk.h"
#include "JobQueue.h"
#include "../Terrain/Terrain.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <mutex>
#include <glm/glm.hpp>

class Loader;
class TerrainTexturePack;
class TerrainTexture;
class Entity;

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

    /// Phase 4 Step 4.2.2 — Streaming radii accessors.
    int activeRadius()   const { return loadRadius_; }
    int loadingRadius()  const { return loadingRadius_; }
    int unloadingRadius()const { return unloadRadius_; }

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

    /// Phase 4 Step 4.2.2 — Three streaming radii:
    int loadRadius_    = 1;   ///< Active: fully simulated & rendered.
    int loadingRadius_ = 2;   ///< Loading: trigger background I/O.
    int unloadRadius_  = 3;   ///< Unloading: destroy entities & free buffers.

    /// Phase 4 Step 4.2.1 — Background thread pool for async chunk I/O.
    JobQueue                                        jobQueue_{2};
    std::mutex                                      pendingMutex_;
    std::unordered_set<int64_t>                     pendingLoads_;

    int64_t chunkKey(int x, int z) const {
        return (static_cast<int64_t>(x) << 32) | static_cast<int64_t>(static_cast<uint32_t>(z));
    }
};

#endif // ENGINE_CHUNKMANAGER_H

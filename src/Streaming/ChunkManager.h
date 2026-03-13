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
#include "ChunkData.h"
#include "JobQueue.h"
#include "../Terrain/Terrain.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <mutex>
#include <functional>
#include <glm/glm.hpp>

class Loader;
class TerrainTexturePack;
class TerrainTexture;

class ChunkManager {
public:
    /// Terrain tile size in world units — mirrors Terrain::kSize.
    static constexpr float kTerrainSize = Terrain::kSize;

    /// GEA Step 5.1 — Callback invoked on the main thread for each baked
    /// entity after a chunk finishes loading.  The Engine binds this to
    /// spawn visual/ECS entities from pre-baked .dat data.
    /// Step 5.4 — now includes chunk coordinates for per-chunk instancing.
    using EntitySpawnCallback = std::function<void(const BakedEntity&,
                                                    int chunkX, int chunkZ)>;
    void setEntityCallback(EntitySpawnCallback cb) { spawnCallback_ = std::move(cb); }

    /// GEA Step 5.4 — Callback invoked when a chunk is about to be unloaded.
    /// The Engine uses this to purge instanced matrices for the chunk.
    using ChunkUnloadCallback = std::function<void(int chunkX, int chunkZ)>;
    void setUnloadCallback(ChunkUnloadCallback cb) { unloadCallback_ = std::move(cb); }

    ChunkManager(Loader*             loader,
                 TerrainTexturePack* texPack,
                 TerrainTexture*     blendMap,
                 const std::string&  heightmapFile);

    ~ChunkManager();

    /// Update which chunks are loaded based on the player's world position.
    void update(const glm::vec3& playerPosition);

    /// Register a pre-existing (SceneLoader-owned) terrain tile so the
    /// ChunkManager knows its grid cell and won't create a duplicate.
    /// The terrain is NOT deleted when the chunk is unloaded.
    void registerTerrain(Terrain* t);

    /// Collect all Terrain pointers from LOADED chunks.
    std::vector<Terrain*>      getActiveTerrains()      const;

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

    /// Maximum number of GL upload tasks to drain per frame when an
    /// active-radius chunk is still loading asynchronously.
    static constexpr int kUrgentDrainLimit = 32;

    /// Phase 4 Step 4.2.1 — Background thread pool for async chunk I/O.
    JobQueue                                        jobQueue_{2};
    std::mutex                                      pendingMutex_;
    std::unordered_set<int64_t>                     pendingLoads_;

    /// GEA Step 5.1 — Entity spawn callback (set by Engine).
    EntitySpawnCallback spawnCallback_;

    /// GEA Step 5.4 — Chunk unload callback (set by Engine).
    ChunkUnloadCallback unloadCallback_;

    /// GEA Step 5.1 — Read a baked chunk .dat file and return its entities.
    static std::vector<BakedEntity> readBakedEntities(int cx, int cz);

    /// GEA Step 5.1 — Fire spawnCallback_ for each baked entity (main thread).
    void fireBakedSpawns(const std::vector<BakedEntity>& bakedEntities,
                          int chunkX, int chunkZ);

    int64_t chunkKey(int x, int z) const {
        return (static_cast<int64_t>(x) << 32) | static_cast<int64_t>(static_cast<uint32_t>(z));
    }
};

#endif // ENGINE_CHUNKMANAGER_H

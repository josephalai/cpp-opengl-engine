// src/Streaming/ChunkManager.cpp

#include "ChunkManager.h"
#include "../Terrain/Terrain.h"
#include "../Entities/Entity.h"
#include <cmath>
#include <algorithm>

ChunkManager::ChunkManager(Loader*             loader,
                            TerrainTexturePack* texPack,
                            TerrainTexture*     blendMap,
                            const std::string&  heightmapFile)
    : loader_(loader)
    , texPack_(texPack)
    , blendMap_(blendMap)
    , heightmapFile_(heightmapFile)
{}

ChunkManager::~ChunkManager() {
    shutdown();
}

void ChunkManager::update(const glm::vec3& playerPos) {
    int px = static_cast<int>(std::floor(playerPos.x / kTerrainSize));
    int pz = static_cast<int>(std::floor(playerPos.z / kTerrainSize));

    // Phase 4 Step 4.2.2 — Active radius: load synchronously (already loaded).
    for (int dx = -loadRadius_; dx <= loadRadius_; ++dx) {
        for (int dz = -loadRadius_; dz <= loadRadius_; ++dz) {
            int cx = px + dx;
            int cz = pz + dz;
            auto key = std::make_pair(cx, cz);

            auto it = chunks_.find(key);
            if (it == chunks_.end()) {
                auto* chunk = new StreamingChunk(cx, cz);
                chunk->load(loader_, texPack_, blendMap_, heightmapFile_);
                chunks_[key] = chunk;
            } else if (it->second->state == StreamingChunk::State::UNLOADED) {
                it->second->load(loader_, texPack_, blendMap_, heightmapFile_);
            }
        }
    }

    // Phase 4 Step 4.2.2 — Loading radius: trigger background I/O for chunks
    // that are 2 cells out.  Heightmap parsing is pushed to the JobQueue
    // background threads; only the final GL buffer upload runs on the main
    // thread via GLUploadQueue.
    for (int dx = -loadingRadius_; dx <= loadingRadius_; ++dx) {
        for (int dz = -loadingRadius_; dz <= loadingRadius_; ++dz) {
            int chebyshev = std::max(std::abs(dx), std::abs(dz));
            if (chebyshev <= loadRadius_) continue; // already handled above

            int cx = px + dx;
            int cz = pz + dz;
            auto key = std::make_pair(cx, cz);

            auto it = chunks_.find(key);
            if (it == chunks_.end()) {
                // Mark as pending to avoid duplicate loads.
                int64_t ck = chunkKey(cx, cz);
                {
                    std::lock_guard<std::mutex> lock(pendingMutex_);
                    if (pendingLoads_.count(ck)) continue;
                    pendingLoads_.insert(ck);
                }
                // Create the chunk in LOADING state on the main thread so
                // subsequent frames don't re-enqueue it.
                auto* chunk = new StreamingChunk(cx, cz);
                chunk->state = StreamingChunk::State::LOADING;
                chunks_[key] = chunk;

                // Push the heavy heightmap load to a background thread.
                // NOTE: terrain->load() currently calls GL functions,
                // so for now the actual load is still performed on this
                // thread.  When the Terrain constructor is refactored to
                // separate CPU parsing from GL upload, the lambda body
                // below should call parseCPU() and enqueue the GPU upload
                // via GLUploadQueue::instance().enqueue().
                jobQueue_.push([this, chunk, cx, cz, key, ck]() {
                    chunk->load(loader_, texPack_, blendMap_, heightmapFile_);
                    {
                        std::lock_guard<std::mutex> lock(pendingMutex_);
                        pendingLoads_.erase(ck);
                    }
                });
            }
        }
    }

    // Unload chunks beyond unloadRadius.
    // Skip chunks in LOADING state — they are being processed by a
    // background thread and must not be deleted until the load completes.
    std::vector<std::pair<int,int>> toRemove;
    for (auto& [key, chunk] : chunks_) {
        if (!chunk || chunk->state == StreamingChunk::State::LOADING) continue;
        int distX = std::abs(key.first  - px);
        int distZ = std::abs(key.second - pz);
        int chebyshev = std::max(distX, distZ);
        if (chebyshev > unloadRadius_) {
            chunk->unload();
            toRemove.push_back(key);
        }
    }
    for (const auto& key : toRemove) {
        delete chunks_[key];
        chunks_.erase(key);
    }
}

void ChunkManager::registerTerrain(Terrain* t) {
    // Derive grid coords from terrain world-space origin.
    int cx = static_cast<int>(std::floor(t->getX() / kTerrainSize));
    int cz = static_cast<int>(std::floor(t->getZ() / kTerrainSize));
    auto key = std::make_pair(cx, cz);

    auto it = chunks_.find(key);
    if (it != chunks_.end()) {
        // Chunk already exists — update its terrain if not already set.
        if (!it->second->terrain) {
            it->second->setExternalTerrain(t);
        }
    } else {
        auto* chunk = new StreamingChunk(cx, cz);
        chunk->setExternalTerrain(t);
        chunks_[key] = chunk;
    }
}

void ChunkManager::registerEntity(Entity* e, const glm::vec3& worldPos) {
    int cx = static_cast<int>(std::floor(worldPos.x / kTerrainSize));
    int cz = static_cast<int>(std::floor(worldPos.z / kTerrainSize));
    auto key = std::make_pair(cx, cz);

    auto it = chunks_.find(key);
    if (it != chunks_.end()) {
        it->second->addEntity(e);
    } else {
        // Create a shell chunk (no terrain) to hold the entity.
        auto* chunk = new StreamingChunk(cx, cz);
        chunk->state = StreamingChunk::State::LOADED;
        chunk->addEntity(e);
        chunks_[key] = chunk;
    }
}

void ChunkManager::removeEntity(Entity* e) {
    for (auto& [key, chunk] : chunks_) {
        if (!chunk) continue;
        auto& ents = chunk->entities;
        auto it = std::find(ents.begin(), ents.end(), e);
        if (it != ents.end()) {
            ents.erase(it);
            return;
        }
    }
}

void ChunkManager::refreshEntityPositions() {
    // Collect entities that need to move to a different chunk.
    // We gather them first to avoid mutating chunk entity lists while
    // iterating over them.
    struct EntityMove { Entity* entity; int chunkX; int chunkZ; };
    std::vector<EntityMove> toMove;

    for (auto& [key, chunk] : chunks_) {
        if (!chunk || chunk->state != StreamingChunk::State::LOADED) continue;
        for (Entity* e : chunk->entities) {
            if (!e) continue;
            const glm::vec3& pos = e->getPosition();
            int cx = static_cast<int>(std::floor(pos.x / kTerrainSize));
            int cz = static_cast<int>(std::floor(pos.z / kTerrainSize));
            if (cx != key.first || cz != key.second) {
                toMove.push_back({e, cx, cz});
            }
        }
    }

    // Move each entity to its correct chunk.
    for (auto& [entity, chunkX, chunkZ] : toMove) {
        removeEntity(entity);
        registerEntity(entity, entity->getPosition());
    }
}

std::vector<Terrain*> ChunkManager::getActiveTerrains() const {
    std::vector<Terrain*> result;
    for (const auto& [key, chunk] : chunks_) {
        if (chunk->state == StreamingChunk::State::LOADED && chunk->terrain) {
            result.push_back(chunk->terrain);
        }
    }
    return result;
}

std::vector<Entity*> ChunkManager::getActiveEntities() const {
    std::vector<Entity*> result;
    for (const auto& [key, chunk] : chunks_) {
        if (chunk->state == StreamingChunk::State::LOADED) {
            result.insert(result.end(), chunk->entities.begin(), chunk->entities.end());
        }
    }
    return result;
}

void ChunkManager::shutdown() {
    for (auto& [key, chunk] : chunks_) {
        if (chunk) {
            chunk->unload();
            delete chunk;
        }
    }
    chunks_.clear();
}

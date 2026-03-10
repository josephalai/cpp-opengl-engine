// src/Streaming/ChunkManager.cpp

#include "ChunkManager.h"
#include "../Terrain/Terrain.h"
#include "../Entities/Entity.h"
#include "../Engine/GLUploadQueue.h"
#include "../Util/FileSystem.h"
#include <cmath>
#include <algorithm>
#include <iostream>

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
            bool justLoaded = false;
            if (it == chunks_.end()) {
                auto* chunk = new StreamingChunk(cx, cz);
                chunk->load(loader_, texPack_, blendMap_, heightmapFile_);
                chunks_[key] = chunk;
                justLoaded = true;
            } else if (it->second->state == StreamingChunk::State::UNLOADED) {
                it->second->load(loader_, texPack_, blendMap_, heightmapFile_);
                justLoaded = true;
            }

            // GEA Step 5.1 / 5.4 — Spawn baked entities for newly loaded chunks
            // AND for pre-registered chunks (via registerTerrain) that haven't
            // had their baked entities spawned yet.
            {
                auto* chunk = chunks_[key];
                if (chunk && chunk->state == StreamingChunk::State::LOADED
                    && !chunk->bakedSpawned) {
                    fireBakedSpawns(readBakedEntities(cx, cz), cx, cz);
                    chunk->bakedSpawned = true;
                }
            }
        }
    }

    // Phase 4 Step 4.2 — Loading radius: truly asynchronous chunk I/O.
    // The background thread calls Terrain::parseCPU() which reads the
    // heightmap image and fills a TerrainData struct (CPU only — no GL calls).
    // It then enqueues the GL upload to GLUploadQueue so the main thread
    // calls loadToVAO() in a microsecond-level time slice.
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

                // Push CPU-only heightmap parsing to a background thread.
                // When parsing completes, enqueue the GL upload to
                // GLUploadQueue so it runs on the main thread.
                jobQueue_.push([this, chunk, cx, cz, ck]() {
                    TerrainData data = Terrain::parseCPU(cx, cz, heightmapFile_);

                    // GEA Step 5.1 — Read baked entity data on the background
                    // thread (pure I/O, no GL).
                    auto bakedEntities = readBakedEntities(cx, cz);

                    {
                        std::lock_guard<std::mutex> lock(pendingMutex_);
                        pendingLoads_.erase(ck);
                    }
                    // Enqueue the GL upload to the main thread.
                    GLUploadQueue::instance().enqueue(
                        [this, chunk, data = std::move(data),
                         baked = std::move(bakedEntities)]() mutable {
                            chunk->finalizeAsync(data, loader_, texPack_, blendMap_);

                            // ---> ADD THIS LOG <---
                            std::cout << "[ChunkManager] STREAMED IN Chunk [" 
                                      << chunk->gridX << ", " << chunk->gridZ << "]\n";

                            // GEA Step 5.1 — Spawn baked entities on the main
                            if (chunk->state == StreamingChunk::State::LOADED) {
                                fireBakedSpawns(baked, chunk->gridX, chunk->gridZ);
                                chunk->bakedSpawned = true;
                            }
                        });
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
            
            // ---> ADD THIS LOG <---
            std::cout << "[ChunkManager] STREAMED OUT Chunk [" 
                      << key.first << ", " << key.second << "]\n";

            // GEA Step 5.4 — Fire unload callback before destroying the chunk.
            if (unloadCallback_) {
                unloadCallback_(key.first, key.second);
            }
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

// ---------------------------------------------------------------------------
// GEA Step 5.1 — Baked chunk helpers
// ---------------------------------------------------------------------------

std::vector<BakedEntity> ChunkManager::readBakedEntities(int cx, int cz) {
    std::string filename = "chunk_" + std::to_string(cx)
                         + "_" + std::to_string(cz) + ".dat";
    std::string path = FileSystem::BakedChunk(filename);
    BakedChunkHeader header{};
    std::vector<BakedEntity> entities;
    readBakedChunk(path, header, entities);
    return entities;
}

void ChunkManager::fireBakedSpawns(const std::vector<BakedEntity>& bakedEntities,
                                    int chunkX, int chunkZ) {
    std::cout << "[ChunkManager] fireBakedSpawns chunk [" << chunkX << ", " << chunkZ
              << "] — " << bakedEntities.size() << " entities, callback="
              << (spawnCallback_ ? "SET" : "NULL") << "\n";
    if (!spawnCallback_) return;
    for (const auto& be : bakedEntities) {
        spawnCallback_(be, chunkX, chunkZ);
    }
}

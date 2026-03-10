// src/Streaming/StreamingChunk.cpp

#include "StreamingChunk.h"
#include "../Terrain/Terrain.h"
#include "../Entities/Entity.h"
#include "../RenderEngine/Loader.h"
#include "../Textures/TerrainTexturePack.h"
#include "../Textures/TerrainTexture.h"

void StreamingChunk::load(Loader* loader,
                           TerrainTexturePack* texPack,
                           TerrainTexture*     blendMap,
                           const std::string&  heightmapFile) {
    if (state == State::LOADED) return;
    state         = State::LOADING;
    terrain       = new Terrain(gridX, gridZ, loader, texPack, blendMap, heightmapFile);
    terrainOwned_ = true;
    state         = State::LOADED;
}

void StreamingChunk::finalizeAsync(const TerrainData& data, Loader* loader,
                                    TerrainTexturePack* texPack, TerrainTexture* blendMap) {
    // Guard: if the chunk was already finalised (e.g. by a synchronous load
    // that ran while the async job was still in-flight), skip re-creation.
    if (state == State::LOADED) return;
    if (data.valid) {
        terrain       = new Terrain(data, loader, texPack, blendMap);
        terrainOwned_ = true;
    }
    state = State::LOADED;
}

void StreamingChunk::setExternalTerrain(Terrain* t) {
    terrain       = t;
    terrainOwned_ = false;
    state         = State::LOADED;
}

void StreamingChunk::addEntity(Entity* e) {
    entities.push_back(e);
}

void StreamingChunk::unload() {
    state = State::UNLOADING;
    if (terrainOwned_) {
        delete terrain;
    }
    terrain = nullptr;
    // NOTE: entities are NOT deleted here because they may be owned by the
    // scene (SceneLoader allocated them). The caller is responsible for
    // entity lifetimes.
    entities.clear();
    state = State::UNLOADED;
}

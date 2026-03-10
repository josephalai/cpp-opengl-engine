//
// Created by Joseph Alai on 7/4/21.
//

#ifndef ENGINE_TERRAIN_H
#define ENGINE_TERRAIN_H

#include "../Models/RawModel.h"
#include "../Textures/ModelTexture.h"
#include "../RenderEngine/Loader.h"
#include "../Textures/TerrainTexturePack.h"
#include "HeightMap.h"
#include "TerrainData.h"
#include <vector>

class Terrain {
private:
    const float kTerrainSize = 800;

public:
    /// World-space size (width and depth) of each terrain tile.
    static constexpr float kSize = 800.0f;

    float x;
    float z;
    RawModel *model;
    TerrainTexturePack *texturePack;
    TerrainTexture *blendMap;
    Heightmap heightMap;
public:

    Heightmap getHeightMap();

    std::vector<std::vector<float>> heights;

    float getSize() const { return kTerrainSize; }

    float getX() const {
        return x;
    }

    void setX(float x) {
        this->x = x;
    }

    float getZ() const {
        return z;
    }

    void setZ(float z) {
        this->z = z;
    }

    RawModel *getModel() const {
        return model;
    }

    void setModel(RawModel *model) {
        this->model = model;
    }

    TerrainTexturePack *getTexturePack() {
        return this->texturePack;
    }

    TerrainTexture *getBlendMap() {
        return this->blendMap;
    }

    float getHeightOfTerrain(float worldX, float worldZ);

    /// Original constructor — parses heightmap AND uploads to GPU (synchronous).
    Terrain(int gridX, int gridZ, Loader *loader, TerrainTexturePack *texturePack, TerrainTexture *blendMap,
            const std::string &heightMap);

    /// Phase 4 Step 4.2 — Construct from pre-parsed CPU data + GPU upload.
    /// The TerrainData must have been produced by parseCPU().
    Terrain(const TerrainData& data, Loader *loader,
            TerrainTexturePack *texturePack, TerrainTexture *blendMap);

    /// Phase 4 Step 4.2 — Parse heightmap into CPU-only arrays (no GL calls).
    /// Safe to call from a background thread.
    static TerrainData parseCPU(int gridX, int gridZ, const std::string& heightmapPath);

    /// Phase 4 Step 4.2 — Upload a TerrainData to the GPU.  Must be called on
    /// the GL thread.
    static RawModel* uploadGPU(Loader* loader, const TerrainData& data);

private:

    RawModel *generateTerrain(Loader *loader);

    glm::vec3 calculateNormal(int x, int z);
};

#endif //ENGINE_TERRAIN_H

// src/Terrain/TerrainData.h
//
// Phase 4 Step 4.2 — CPU-side terrain data transfer object.
// Holds all vertex/normal/UV/index arrays parsed from a heightmap on a
// background thread.  No OpenGL or GLFW dependencies.

#ifndef ENGINE_TERRAINDATA_H
#define ENGINE_TERRAINDATA_H

#include <vector>
#include <cstdint>

struct TerrainData {
    std::vector<float>  vertices;
    std::vector<float>  normals;
    std::vector<float>  textureCoords;
    std::vector<int32_t> indices;
    std::vector<std::vector<float>> heights;
    int   gridX  = 0;
    int   gridZ  = 0;
    float originX = 0.0f;
    float originZ = 0.0f;
    bool  valid   = false;
};

#endif // ENGINE_TERRAINDATA_H

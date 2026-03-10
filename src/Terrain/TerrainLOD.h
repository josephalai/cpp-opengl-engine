// src/Terrain/TerrainLOD.h
//
// Phase 4 Step 4.3.2 — Terrain GeoMipMapping.
// Breaks the terrain into 50×50-unit patches and generates index buffers
// at multiple LOD levels.  Far patches skip every other vertex (reducing
// triangle count by 75%).
//
// Usage (client-side only):
//   TerrainLOD lod;
//   lod.build(vertexCount, terrainSize, patchSize);
//   auto& indices = lod.getIndicesForPatch(patchX, patchZ, lodLevel);
//
// This is a header-only utility so it can be included without adding a
// .cpp file to the build.  All methods are marked inline.

#ifndef ENGINE_TERRAINLOD_H
#define ENGINE_TERRAINLOD_H

#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>

class TerrainLOD {
public:
    /// Number of LOD levels (0 = full detail, 1 = half, 2 = quarter).
    static constexpr int kMaxLODLevels = 3;

    /// Build LOD index buffers for the terrain.
    /// @param vertexCount   Number of vertices per row/column of the heightmap.
    /// @param terrainSize   World-space size of the terrain tile (e.g. 800 m).
    /// @param patchSize     World-space size of each LOD patch (e.g. 50 m).
    void build(int vertexCount, float terrainSize, float patchSize = 50.0f) {
        vertexCount_ = vertexCount;
        terrainSize_ = terrainSize;
        patchSize_   = patchSize;
        patchesPerRow_ = static_cast<int>(std::ceil(terrainSize / patchSize));

        // Vertex spacing in grid units.
        float vertexSpacing = terrainSize / static_cast<float>(vertexCount - 1);
        int vertsPerPatch = static_cast<int>(std::ceil(patchSize / vertexSpacing));

        for (int lod = 0; lod < kMaxLODLevels; ++lod) {
            int step = 1 << lod;  // LOD0: 1, LOD1: 2, LOD2: 4

            for (int pz = 0; pz < patchesPerRow_; ++pz) {
                for (int px = 0; px < patchesPerRow_; ++px) {
                    int startX = px * vertsPerPatch;
                    int startZ = pz * vertsPerPatch;
                    int endX   = std::min(startX + vertsPerPatch, vertexCount - 1);
                    int endZ   = std::min(startZ + vertsPerPatch, vertexCount - 1);

                    std::vector<int32_t> indices;
                    for (int gz = startZ; gz < endZ; gz += step) {
                        for (int gx = startX; gx < endX; gx += step) {
                            int topLeft     = gz * vertexCount + gx;
                            int topRight    = gz * vertexCount + std::min(gx + step, endX);
                            int bottomLeft  = std::min(gz + step, endZ) * vertexCount + gx;
                            int bottomRight = std::min(gz + step, endZ) * vertexCount
                                            + std::min(gx + step, endX);

                            // First triangle.
                            indices.push_back(topLeft);
                            indices.push_back(bottomLeft);
                            indices.push_back(topRight);
                            // Second triangle.
                            indices.push_back(topRight);
                            indices.push_back(bottomLeft);
                            indices.push_back(bottomRight);
                        }
                    }

                    int64_t key = makePatchKey(px, pz, lod);
                    patchIndices_[key] = std::move(indices);
                }
            }
        }
        built_ = true;
    }

    /// Get the index buffer for a specific patch at a specific LOD level.
    const std::vector<int32_t>& getIndicesForPatch(int patchX, int patchZ,
                                                    int lodLevel) const {
        static const std::vector<int32_t> empty;
        int64_t key = makePatchKey(patchX, patchZ, lodLevel);
        auto it = patchIndices_.find(key);
        return (it != patchIndices_.end()) ? it->second : empty;
    }

    /// Determine which LOD level to use for a patch based on camera distance.
    /// @param patchCenterX, patchCenterZ  World-space patch center.
    /// @param cameraX, cameraZ            Camera world position.
    int selectLOD(float patchCenterX, float patchCenterZ,
                  float cameraX, float cameraZ) const {
        float dx = patchCenterX - cameraX;
        float dz = patchCenterZ - cameraZ;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist < patchSize_ * 3.0f)  return 0;  // Full detail within ~150 m
        if (dist < patchSize_ * 8.0f)  return 1;  // Half detail within ~400 m
        return 2;                                   // Quarter detail beyond
    }

    int patchesPerRow() const { return patchesPerRow_; }
    float patchSize()   const { return patchSize_; }
    bool isBuilt()      const { return built_; }

private:
    int   vertexCount_   = 0;
    float terrainSize_   = 800.0f;
    float patchSize_     = 50.0f;
    int   patchesPerRow_ = 16;
    bool  built_         = false;

    std::unordered_map<int64_t, std::vector<int32_t>> patchIndices_;

    static int64_t makePatchKey(int px, int pz, int lod) {
        // Pack patch coords + lod into a single int64.
        return (static_cast<int64_t>(lod) << 48)
             | (static_cast<int64_t>(static_cast<uint32_t>(pz)) << 24)
             | static_cast<int64_t>(static_cast<uint32_t>(px));
    }
};

#endif // ENGINE_TERRAINLOD_H

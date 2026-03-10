//
// Created by Joseph Alai on 7/6/21.
//

#include "Terrain.h"
#include "../Toolbox/Maths.h"
#include "../Util/FileSystem.h"
#include <fstream>

/**
 * @brief Terrain generates terrain coordinates and Stores them in a Vao, inputs the texturePack
 *        and the blendMap and stores those too. It also stores the height information for x,z,
 *        so that you can get the Y vertex from any X, Z coordinates.
 *        
 *        It loads in a heightMap image, which is converted into a
 *        3d mesh. There is a grid system, wherein if you want to generate multiple terrains,
 *        you would simply pass the coordinates, [0, 0], [0, 1], [1, 1], [1, 0]..., for each
 *        terrain respectively, to be able to switch between terrains.
 *
 * @param gridX
 * @param gridZ
 * @param loader
 * @param texturePack
 * @param blendMap
 * @param heightMap
 */
Terrain::Terrain(int gridX, int gridZ, Loader *loader, TerrainTexturePack *texturePack, TerrainTexture *blendMap,
                 const std::string &heightMap) : heightMap(Heightmap(FileSystem::Texture(heightMap))) {
    this->texturePack = texturePack;
    this->blendMap = blendMap;
    this->x = static_cast<float>( gridX) * kTerrainSize;
    this->z = static_cast<float>( gridZ) * kTerrainSize;
    this->model = generateTerrain(loader);
}

/// Phase 4 Step 4.2 — Construct from pre-parsed TerrainData + immediate GPU upload.
/// The Heightmap member is initialised with an empty path (safe — it will have
/// no height data) because all vertex data comes from the pre-parsed TerrainData.
Terrain::Terrain(const TerrainData& data, Loader *loader,
                 TerrainTexturePack *texturePack, TerrainTexture *blendMap)
    : heightMap(Heightmap(""))  // no file needed — data already parsed
{
    this->texturePack = texturePack;
    this->blendMap    = blendMap;
    this->x           = data.originX;
    this->z           = data.originZ;
    this->heights     = data.heights;
    this->model       = uploadGPU(loader, data);
}

/// Phase 4 Step 4.2 — CPU-only heightmap parsing.  Safe for background threads.
/// Step 3 — Multi-tile: tries heightmapPath_X_Z.png first, falls back to base.
TerrainData Terrain::parseCPU(int gridX, int gridZ, const std::string& heightmapPath) {
    TerrainData out;
    out.gridX  = gridX;
    out.gridZ  = gridZ;
    out.originX = static_cast<float>(gridX) * kSize;
    out.originZ = static_cast<float>(gridZ) * kSize;

    // Try per-tile file first: e.g. "heightMap_0_-1"
    std::string perTileName = heightmapPath + "_" + std::to_string(gridX) + "_" + std::to_string(gridZ);
    std::string perTilePath = FileSystem::Texture(perTileName);
    // Check if per-tile file exists by probing the file.
    std::string resolvedPath;
    {
        std::ifstream probe(perTilePath);
        if (probe.is_open()) {
            resolvedPath = perTilePath;
            probe.close();
        } else {
            // Fall back to the base heightmap.
            resolvedPath = FileSystem::Texture(heightmapPath);
        }
    }

    Heightmap hm(resolvedPath);
    ImageInfo info = hm.getImageInfo();
    int vertexCount = info.height;
    if (vertexCount <= 0) {
        // Heightmap failed to load — return invalid data.
        return out;
    }

    out.heights.resize(vertexCount, std::vector<float>(vertexCount));
    int count = vertexCount * vertexCount;

    out.vertices.resize(count * 3);
    out.normals.resize(count * 3);
    out.textureCoords.resize(count * 2);
    out.indices.resize(6 * (vertexCount - 1) * (vertexCount - 1));

    int vertexPointer = 0;
    for (int i = 0; i < vertexCount; i++) {
        for (int j = 0; j < vertexCount; j++) {
            out.vertices[vertexPointer * 3] =
                static_cast<float>(j) / (static_cast<float>(vertexCount) - 1) * kSize;
            float height = hm.getHeight(j, i);
            out.heights[j][i] = height;
            out.vertices[vertexPointer * 3 + 1] = height;
            out.vertices[vertexPointer * 3 + 2] =
                static_cast<float>(i) / (static_cast<float>(vertexCount) - 1) * kSize;

            // Calculate normal inline (same logic as calculateNormal).
            float heightL = hm.getHeight(j - 1, i);
            float heightR = hm.getHeight(j + 1, i);
            float heightD = hm.getHeight(j, i - 1);
            float heightU = hm.getHeight(j, i + 1);
            glm::vec3 normal = glm::normalize(glm::vec3(heightL - heightR, 2.0f, heightD - heightU));

            out.normals[vertexPointer * 3]     = normal.x;
            out.normals[vertexPointer * 3 + 1] = normal.y;
            out.normals[vertexPointer * 3 + 2] = normal.z;

            out.textureCoords[vertexPointer * 2] =
                static_cast<float>(j) / (static_cast<float>(vertexCount) - 1);
            out.textureCoords[vertexPointer * 2 + 1] =
                static_cast<float>(i) / (static_cast<float>(vertexCount) - 1);
            vertexPointer++;
        }
    }

    int pointer = 0;
    for (int gz = 0; gz < vertexCount - 1; gz++) {
        for (int gx = 0; gx < vertexCount - 1; gx++) {
            int topLeft     = (gz * vertexCount) + gx;
            int topRight    = topLeft + 1;
            int bottomLeft  = ((gz + 1) * vertexCount) + gx;
            int bottomRight = bottomLeft + 1;
            out.indices[pointer++] = topLeft;
            out.indices[pointer++] = bottomLeft;
            out.indices[pointer++] = topRight;
            out.indices[pointer++] = topRight;
            out.indices[pointer++] = bottomLeft;
            out.indices[pointer++] = bottomRight;
        }
    }

    out.valid = true;
    return out;
}

/// Phase 4 Step 4.2 — Upload pre-parsed terrain data to the GPU.
RawModel* Terrain::uploadGPU(Loader* loader, const TerrainData& data) {
    return loader->loadToVAO(data.vertices, data.textureCoords, data.normals, data.indices);
}

RawModel *Terrain::generateTerrain(Loader *loader) {
    int vertexCount = heightMap.getImageInfo().height;
    heights = std::vector<std::vector<float>>(vertexCount, std::vector<float>(vertexCount));

    int count = vertexCount * vertexCount;

    std::vector<GLfloat> vertices(count * 3);
    vertices.reserve(count * 3);
    std::vector<GLfloat> normals(count * 3);
    normals.reserve(count * 3);
    std::vector<GLfloat> textureCoords(count * 2);
    textureCoords.reserve(count * 2);
    std::vector<GLint> indices(6 * (vertexCount - 1) * (vertexCount - 1));
    indices.reserve(6 * (vertexCount - 1) * (vertexCount - 1));

    int vertexPointer = 0;
    for (int i = 0; i < vertexCount; i++) {
        for (int j = 0; j < vertexCount; j++) {
            vertices[vertexPointer * 3] = static_cast<float>(j) / (static_cast<float>(vertexCount) - 1) * kTerrainSize;
            float height = heightMap.getHeight(j, i);
            heights[j][i] = height;
            vertices[vertexPointer * 3 + 1] = height;
            vertices[vertexPointer * 3 + 2] = static_cast<float>(i) / (static_cast<float>(vertexCount) - 1) * kTerrainSize;
            glm::vec3 normal = calculateNormal(j, i);
            normals[vertexPointer * 3] = normal.x;
            normals[vertexPointer * 3 + 1] = normal.y;
            normals[vertexPointer * 3 + 2] = normal.z;
            textureCoords[vertexPointer * 2] = static_cast<float>(j) / (static_cast<float>(vertexCount) - 1);
            textureCoords[vertexPointer * 2 + 1] = static_cast<float>(i) / (static_cast<float>(vertexCount) - 1);
            vertexPointer++;
        }
    }
    int pointer = 0;
    for (int gz = 0; gz < vertexCount - 1; gz++) {
        for (int gx = 0; gx < vertexCount - 1; gx++) {
            int topLeft = (gz * vertexCount) + gx;
            int topRight = topLeft + 1;
            int bottomLeft = ((gz + 1) * vertexCount) + gx;
            int bottomRight = bottomLeft + 1;
            indices[pointer++] = topLeft;
            indices[pointer++] = bottomLeft;
            indices[pointer++] = topRight;
            indices[pointer++] = topRight;
            indices[pointer++] = bottomLeft;
            indices[pointer++] = bottomRight;
        }
    }
    return loader->loadToVAO(vertices, textureCoords, normals, indices);
}

glm::vec3 Terrain::calculateNormal(int x, int z) {
    float heightL = heightMap.getHeight(x - 1, z);
    float heightR = heightMap.getHeight(x + 1, z);
    float heightD = heightMap.getHeight(x, z - 1);
    float heightU = heightMap.getHeight(x, z + 1);
    glm::vec3 normal(heightL - heightR, 2.0f, heightD - heightU);
    return glm::normalize(normal);
}

Heightmap Terrain::getHeightMap() {
    return heightMap;
}

float Terrain::getHeightOfTerrain(float worldX, float worldZ) {
    float terrainX = worldX - this->x;
    float terrainZ = worldZ - this->z;
    float gridSquareSize = kTerrainSize / (static_cast<float>(heights.size()) - 1);
    int gridX = static_cast<int> (floor(terrainX / gridSquareSize));
    int gridZ = static_cast<int> (floor(terrainZ / gridSquareSize));

    if (gridX >= heights.size() - 1 || gridZ >= heights.size() - 1 || gridX < 0 || gridZ < 0) {
        return 0;
    }

    float xCoord = fmod(terrainX, gridSquareSize) / gridSquareSize;
    float zCoord = fmod(terrainZ, gridSquareSize) / gridSquareSize;

    if (xCoord <= (1 - zCoord)) {
        return Maths::barryCentric(glm::vec3(0, heights[gridX][gridZ], 0), glm::vec3(1,
                                                                                     heights[gridX + 1][gridZ], 0),
                                   glm::vec3(0,
                                             heights[gridX][gridZ + 1], 1), glm::vec2(xCoord, zCoord));
    } else {
        return Maths::barryCentric(glm::vec3(1, heights[gridX + 1][gridZ], 0), glm::vec3(1,
                                                                                         heights[gridX + 1][gridZ +
                                                                                                            1], 1),
                                   glm::vec3(0,
                                             heights[gridX][gridZ + 1], 1), glm::vec2(xCoord, zCoord));
    }
}
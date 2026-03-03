//
// Created by Joseph Alai on 7/6/21.
//

#include "Terrain.h"
#include "../Toolbox/Maths.h"
#include "../Util/FileSystem.h"

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
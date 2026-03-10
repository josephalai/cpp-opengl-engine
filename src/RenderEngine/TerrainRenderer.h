//
// Created by Joseph Alai on 7/4/21.
//

#ifndef ENGINE_TERRAINRENDERER_H
#define ENGINE_TERRAINRENDERER_H
#include "glm/glm.hpp"
#include "../Shaders/TerrainShader.h"
#include "../Terrain/Terrain.h"
#include "../Terrain/TerrainLOD.h"

class TerrainRenderer {
private:
    TerrainShader *shader;
    TerrainLOD     terrainLOD_;   ///< Phase 4 Step 4.3 — GeoMipMapping.
    bool           lodEnabled_ = false;

public:
    TerrainRenderer(TerrainShader *shader, glm::mat4 projectionMatrix);

    void render(std::vector<Terrain*> *terrains);

    /// Phase 4 Step 4.3.2 — Enable terrain LOD with the given vertex count and tile size.
    void enableLOD(int vertexCount, float terrainSize, float patchSize = 50.0f);

    /// Phase 4 Step 4.3.2 — Set the camera position for LOD distance calculations.
    void setCameraPosition(const glm::vec3& camPos) { cameraPos_ = camPos; }

private:
    glm::vec3 cameraPos_ = glm::vec3(0.0f);

    void prepareTerrain(Terrain *terrain);

    void unbindTexturedModel();

    void loadModelMatrix(Terrain *terrain);

    void bindTextures(Terrain *terrain);
};
#endif //ENGINE_TERRAINRENDERER_H

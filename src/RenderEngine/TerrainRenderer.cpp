//
// Created by Joseph Alai on 7/6/21.
//

#include "TerrainRenderer.h"
#include "../Toolbox/Maths.h"
#include <algorithm>

TerrainRenderer::TerrainRenderer(TerrainShader *shader, glm::mat4 projectionMatrix) {
    this->shader = shader;
    shader->start();
    shader->loadProjectionMatrix(projectionMatrix);
    shader->connectTextureUnits();
    shader->stop();
}

/// Phase 4 Step 4.3.2 — Enable terrain GeoMipMapping.
void TerrainRenderer::enableLOD(int vertexCount, float terrainSize, float patchSize) {
    terrainLOD_.build(vertexCount, terrainSize, patchSize);
    lodEnabled_ = terrainLOD_.isBuilt();
}

void TerrainRenderer::render(std::vector<Terrain *> *terrains) {
    for (Terrain *terrain: *terrains) {
        prepareTerrain(terrain);
        loadModelMatrix(terrain);

        // Phase 4 Step 4.3.2 — GeoMipMapping: reduce triangle count for
        // distant terrain tiles.  When LOD is enabled, select a reduced
        // index count based on camera distance to the tile center.
        if (lodEnabled_) {
            float tileCenterX = terrain->getX() + terrain->getSize() * 0.5f;
            float tileCenterZ = terrain->getZ() + terrain->getSize() * 0.5f;
            int lod = terrainLOD_.selectLOD(tileCenterX, tileCenterZ,
                                             cameraPos_.x, cameraPos_.z);
            // Reduce index count by the square of the LOD step (each level
            // skips every-other vertex in both X and Z, quartering triangles).
            static constexpr int kLODDivisorShift  = 2;  // bits per LOD level (÷4 each step)
            static constexpr int kMinLODIndexCount = 6;  // minimum: one quad (2 triangles)
            int fullCount = terrain->getModel()->getVertexCount();
            int divisor = 1 << (lod * kLODDivisorShift);  // 1, 4, 16
            int lodCount = std::max(fullCount / divisor, kMinLODIndexCount);
            glDrawElements(GL_TRIANGLES, lodCount, GL_UNSIGNED_INT, 0);
        } else {
            glDrawElements(GL_TRIANGLES, terrain->getModel()->getVertexCount(),
                           GL_UNSIGNED_INT, 0);
        }
    }
    unbindTexturedModel();
}

void TerrainRenderer::prepareTerrain(Terrain *terrain) {
    RawModel *rawModel = terrain->getModel();

    // bind the current vao
    glBindVertexArray(rawModel->getVaoId());

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    bindTextures(terrain);
    shader->loadMaterial(terrain->getTexturePack()->getMaterial());
}

/**
 * @brief Binds the textures from the TexturePack to blend them together
 *
 * @param terrain
 */
void TerrainRenderer::bindTextures(Terrain *terrain) {
    TerrainTexturePack *texturePack = terrain->getTexturePack();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texturePack->getBackgroundTexture()->getTextureId());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texturePack->getRTexture()->getTextureId());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texturePack->getGTexture()->getTextureId());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, texturePack->getBTexture()->getTextureId());
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, terrain->getBlendMap()->getTextureId());
}

void TerrainRenderer::unbindTexturedModel() {
    // clean up
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindVertexArray(0);
}

void TerrainRenderer::loadModelMatrix(Terrain *terrain) {
    // creates the matrices to be passed into the shader
    glm::mat4 transformationMatrix = Maths::createTransformationMatrix(
            glm::vec3(terrain->getX(), 0.0f, terrain->getZ()));
    shader->loadTransformationMatrix(transformationMatrix);
}
// WaterRenderer.cpp

#include "WaterRenderer.h"
#include "../Toolbox/Maths.h"
#include "../RenderEngine/DisplayManager.h"

WaterRenderer::WaterRenderer(Loader* loader,
                             WaterShader* shader,
                             const glm::mat4& /*projectionMatrix*/,
                             FrameBuffers* fbos,
                             GLuint dudvTexture,
                             GLuint normalMapTexture)
    : shader_(shader), fbos_(fbos), waterVAO_(0), waterVBO_(0),
      dudvTexture_(dudvTexture), normalMapTexture_(normalMapTexture)
{
    // Build a simple quad VAO for the water plane (XZ, Y handled by transform)
    static const float kVertices[] = { -1.0f, -1.0f,  -1.0f, 1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,  1.0f, 1.0f };

    glGenVertexArrays(1, &waterVAO_);
    glBindVertexArray(waterVAO_);

    glGenBuffers(1, &waterVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void WaterRenderer::render(const std::vector<WaterTile>& tiles,
                            Camera* camera,
                            const glm::mat4& projectionMatrix,
                            Light* sun)
{
    prepareRender(camera, projectionMatrix, sun);

    for (const WaterTile& tile : tiles) {
        prepareTile(tile, projectionMatrix);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    unbind();
}

void WaterRenderer::prepareRender(Camera* camera, const glm::mat4& projectionMatrix, Light* sun) {
    shader_->start();

    shader_->loadProjectionMatrix(projectionMatrix);
    shader_->loadViewMatrix(camera->getViewMatrix());
    shader_->loadCameraPosition(camera->Position);

    if (sun) {
        shader_->loadLightPosition(sun->getPosition());
        shader_->loadLightColor(sun->getColor());
    } else {
        shader_->loadLightPosition(glm::vec3(0.0f, 1000.0f, 0.0f));
        shader_->loadLightColor(glm::vec3(1.0f));
    }

    moveFactor_ += kWaveSpeed * 0.016f;   // ~60 fps assumption; use real delta if available
    if (moveFactor_ > 1.0f) moveFactor_ -= 1.0f;
    shader_->loadMoveFactor(moveFactor_);

    shader_->connectTextureUnits();

    // Bind reflection and refraction textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbos_->getReflectionTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fbos_->getRefractionTexture());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, dudvTexture_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, normalMapTexture_);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, fbos_->getRefractionDepthTexture());

    // Alpha blending for semi-transparent water
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(waterVAO_);
    glEnableVertexAttribArray(0);
}

void WaterRenderer::prepareTile(const WaterTile& tile, const glm::mat4& /*projectionMatrix*/) {
    glm::mat4 transform = Maths::createTransformationMatrix(
        glm::vec3(tile.centerX, tile.height, tile.centerZ),
        glm::vec3(0.0f),
        WaterTile::kTileSize);
    shader_->loadTransformationMatrix(transform);
}

void WaterRenderer::unbind() {
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    shader_->stop();
}

void WaterRenderer::cleanUp() {
    if (waterVAO_) glDeleteVertexArrays(1, &waterVAO_);
    if (waterVBO_) glDeleteBuffers(1, &waterVBO_);
}

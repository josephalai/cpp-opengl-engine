// WaterRenderer.h — renders water tiles using reflection/refraction FBOs with DuDv distortion.

#ifndef ENGINE_WATERRENDERER_H
#define ENGINE_WATERRENDERER_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <vector>

#include "WaterTile.h"
#include "WaterShader.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../RenderEngine/Loader.h"
#include "../Entities/Camera.h"
#include "../Entities/Light.h"

class WaterRenderer {
public:
    static constexpr float kWaveSpeed = 0.03f;

    WaterRenderer(Loader* loader,
                  WaterShader* shader,
                  const glm::mat4& projectionMatrix,
                  FrameBuffers* fbos,
                  GLuint dudvTexture,
                  GLuint normalMapTexture);

    void render(const std::vector<WaterTile>& tiles,
                Camera* camera,
                const glm::mat4& projectionMatrix,
                Light* sun);

    void cleanUp();

    float getMoveFactor() const { return moveFactor_; }

private:
    WaterShader*  shader_;
    FrameBuffers* fbos_;
    GLuint        waterVAO_;
    GLuint        waterVBO_;
    GLuint        dudvTexture_;
    GLuint        normalMapTexture_;
    float         moveFactor_ = 0.0f;

    void prepareRender(Camera* camera, const glm::mat4& projectionMatrix, Light* sun);
    void unbind();
    void prepareTile(const WaterTile& tile, const glm::mat4& projectionMatrix);
};

#endif // ENGINE_WATERRENDERER_H

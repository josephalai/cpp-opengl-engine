// src/Shaders/InstancedShader.h
// Shader for instanced rendering — model matrix supplied per-instance.
// Phase 4 Step 4.3.3 — Adds maxInstanceDistance uniform for GPU-side
// distance culling of individual instances (e.g. grass blades > 75 m).

#ifndef ENGINE_INSTANCEDSHADER_H
#define ENGINE_INSTANCEDSHADER_H

#include "ShaderProgram.h"
#include "../Entities/Camera.h"
#include "../Entities/Light.h"
#include "../Toolbox/Color.h"

class InstancedShader : public ShaderProgram {
public:
    static const int MAX_LIGHTS = 4;

    InstancedShader();

    void loadViewMatrix(const glm::mat4& m);
    void loadProjectionMatrix(const glm::mat4& m);
    void loadViewPosition(Camera* camera);
    void loadLight(const std::vector<Light*>& lights);
    void loadSkyColor(const glm::vec3& color);
    void connectTexture();

    /// Phase 4 Step 4.3.3 — Set the maximum distance at which instances
    /// are drawn.  Instances beyond this distance are collapsed to zero
    /// by the vertex shader.  Set to 0 to disable culling.
    void loadMaxInstanceDistance(float distance);

protected:
    void bindAttributes() override;
    void getAllUniformLocations() override;

private:
    constexpr static const char* VertexPath   = "/src/Shaders/Instanced/VertexShader.glsl";
    constexpr static const char* FragmentPath = "/src/Shaders/Instanced/FragmentShader.glsl";

    GLint loc_viewMatrix;
    GLint loc_projectionMatrix;
    GLint loc_viewPosition;
    GLint loc_skyColor;
    GLint loc_textureSampler;
    GLint loc_maxInstanceDistance;

    GLint loc_materialShininess;
    GLint loc_materialReflectivity;

    GLint loc_lightPosition [MAX_LIGHTS];
    GLint loc_lightDiffuse  [MAX_LIGHTS];
    GLint loc_lightAmbient  [MAX_LIGHTS];
    GLint loc_lightSpecular [MAX_LIGHTS];
    GLint loc_lightConstant [MAX_LIGHTS];
    GLint loc_lightLinear   [MAX_LIGHTS];
    GLint loc_lightQuadratic[MAX_LIGHTS];
};

#endif // ENGINE_INSTANCEDSHADER_H

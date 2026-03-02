// src/Shaders/InstancedShader.h
// Shader for instanced rendering — model matrix supplied per-instance.

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

// src/Shaders/AnimatedShader.h
// Shader for skeletal animated models.

#ifndef ENGINE_ANIMATEDSHADER_H
#define ENGINE_ANIMATEDSHADER_H

#include "ShaderProgram.h"
#include "../Entities/Camera.h"
#include "../Entities/Light.h"
#include "../Toolbox/Color.h"
#include "../Animation/Skeleton.h"

class AnimatedShader : public ShaderProgram {
public:
    static const int MAX_LIGHTS = 4;

    AnimatedShader();

    void loadTransformationMatrix(const glm::mat4& m);
    void loadViewMatrix(const glm::mat4& m);
    void loadProjectionMatrix(const glm::mat4& m);
    void loadViewPosition(Camera* camera);
    void loadLight(const std::vector<Light*>& lights);
    void loadSkyColor(const glm::vec3& color);
    void loadBoneMatrices(const std::vector<glm::mat4>& matrices);

protected:
    void bindAttributes() override;
    void getAllUniformLocations() override;

private:
    constexpr static const char* VertexPath   = "/src/Shaders/Animated/VertexShader.glsl";
    constexpr static const char* FragmentPath = "/src/Shaders/Animated/FragmentShader.glsl";

    GLint loc_transformationMatrix;
    GLint loc_viewMatrix;
    GLint loc_projectionMatrix;
    GLint loc_viewPosition;
    GLint loc_skyColor;
    GLint loc_textureSampler;

    GLint loc_lightPosition [MAX_LIGHTS];
    GLint loc_lightDiffuse  [MAX_LIGHTS];
    GLint loc_lightAmbient  [MAX_LIGHTS];
    GLint loc_lightSpecular [MAX_LIGHTS];
    GLint loc_lightConstant [MAX_LIGHTS];
    GLint loc_lightLinear   [MAX_LIGHTS];
    GLint loc_lightQuadratic[MAX_LIGHTS];

    GLint loc_materialShininess;
    GLint loc_materialReflectivity;

    GLint loc_boneMatrices[MAX_BONES];
};

#endif // ENGINE_ANIMATEDSHADER_H

// src/Shaders/AnimatedShader.h
// Shader for skeletal animated models.
// Bone matrices are uploaded via a UBO (BoneBuffer) instead of individual
// uniform locations, which scales to larger bone counts and reduces driver overhead.

#ifndef ENGINE_ANIMATEDSHADER_H
#define ENGINE_ANIMATEDSHADER_H

#include "ShaderProgram.h"
#include "../Entities/Camera.h"
#include "../Entities/Light.h"
#include "../Toolbox/Color.h"
#include "../Animation/Skeleton.h"
#include "../Animation/BoneBuffer.h"

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

    /// Upload bone matrices via the managed UBO and bind it to the shader block.
    void loadBoneMatrices(const std::vector<glm::mat4>& matrices);

    /// Access the underlying BoneBuffer (e.g. to call cleanup on shutdown).
    BoneBuffer& getBoneBuffer() { return boneBuffer_; }

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

    /// UBO-based bone matrix palette — avoids per-bone glUniformMatrix4fv calls.
    BoneBuffer boneBuffer_;
};

#endif // ENGINE_ANIMATEDSHADER_H

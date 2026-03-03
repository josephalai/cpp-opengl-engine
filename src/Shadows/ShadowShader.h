// ShadowShader.h — depth-only shader used for the shadow map pass.

#ifndef ENGINE_SHADOWSHADER_H
#define ENGINE_SHADOWSHADER_H

#include "../Shaders/ShaderProgram.h"

class ShadowShader : public ShaderProgram {
public:
    ShadowShader();

    void loadLightSpaceMatrix(const glm::mat4& m);
    void loadTransformationMatrix(const glm::mat4& m);

protected:
    void bindAttributes() override;
    void getAllUniformLocations() override;

private:
    constexpr static const char* VertexPath   = "/src/Shadows/ShadowVertex.glsl";
    constexpr static const char* FragmentPath = "/src/Shadows/ShadowFragment.glsl";

    GLint loc_lightSpaceMatrix;
    GLint loc_transformationMatrix;
};

#endif // ENGINE_SHADOWSHADER_H

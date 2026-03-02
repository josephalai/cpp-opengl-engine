// WaterShader.h — shader for water rendering (reflection + refraction + DuDv distortion).

#ifndef ENGINE_WATERSHADER_H
#define ENGINE_WATERSHADER_H

#include "../Shaders/ShaderProgram.h"
#include "../Entities/Camera.h"
#include "../Entities/Light.h"

class WaterShader : public ShaderProgram {
public:
    WaterShader();

    void loadProjectionMatrix(const glm::mat4& m);
    void loadViewMatrix(const glm::mat4& m);
    void loadTransformationMatrix(const glm::mat4& m);
    void loadCameraPosition(const glm::vec3& pos);
    void loadLightPosition(const glm::vec3& pos);
    void loadLightColor(const glm::vec3& color);
    void loadMoveFactor(float factor);
    void connectTextureUnits();

protected:
    void bindAttributes() override;
    void getAllUniformLocations() override;

private:
    constexpr static const char* VertexPath   = "/src/Water/Shaders/WaterVertex.glsl";
    constexpr static const char* FragmentPath = "/src/Water/Shaders/WaterFragment.glsl";

    GLint loc_projectionMatrix;
    GLint loc_viewMatrix;
    GLint loc_transformationMatrix;
    GLint loc_cameraPosition;
    GLint loc_lightPosition;
    GLint loc_lightColor;
    GLint loc_moveFactor;
    GLint loc_reflectionTexture;
    GLint loc_refractionTexture;
    GLint loc_dudvMap;
    GLint loc_normalMap;
    GLint loc_depthMap;
};

#endif // ENGINE_WATERSHADER_H

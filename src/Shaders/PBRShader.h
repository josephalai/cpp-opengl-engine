// PBRShader.h — Cook-Torrance metallic-roughness PBR shader.

#ifndef ENGINE_PBRSHADER_H
#define ENGINE_PBRSHADER_H

#include "../Shaders/ShaderProgram.h"
#include "../Entities/Camera.h"
#include "../Entities/Light.h"
#include "../Toolbox/Color.h"

class PBRShader : public ShaderProgram {
public:
    static const int MAX_LIGHTS = 4;

    PBRShader();

    void loadTransformationMatrix(const glm::mat4& m);
    void loadViewMatrix(const glm::mat4& m);
    void loadProjectionMatrix(const glm::mat4& m);
    void loadViewPosition(Camera* camera);
    void loadLight(const std::vector<Light*>& lights);
    void loadSkyColor(Color& c);
    void loadFogDensity(float density);

    // PBR material uniforms
    void loadAlbedoValue(const glm::vec3& v);
    void loadMetallicValue(float v);
    void loadRoughnessValue(float v);
    void loadAOValue(float v);
    void loadUseAlbedoMap(bool v);
    void loadUseNormalMap(bool v);
    void loadUseMetallicMap(bool v);
    void loadUseRoughnessMap(bool v);
    void loadUseAOMap(bool v);
    void connectTextureUnits();

protected:
    void bindAttributes() override;
    void getAllUniformLocations() override;

private:
    constexpr static const char* VertexPath   = "/src/Shaders/PBR/VertexShader.glsl";
    constexpr static const char* FragmentPath = "/src/Shaders/PBR/FragmentShader.glsl";

    // Uniform locations
    GLint loc_transformationMatrix;
    GLint loc_viewMatrix;
    GLint loc_projectionMatrix;
    GLint loc_viewPosition;
    GLint loc_skyColor;
    GLint loc_fogDensity;

    GLint loc_albedoValue;
    GLint loc_metallicValue;
    GLint loc_roughnessValue;
    GLint loc_aoValue;
    GLint loc_useAlbedoMap;
    GLint loc_useNormalMap;
    GLint loc_useMetallicMap;
    GLint loc_useRoughnessMap;
    GLint loc_useAOMap;

    GLint loc_albedoMap;
    GLint loc_normalMap;
    GLint loc_metallicMap;
    GLint loc_roughnessMap;
    GLint loc_aoMap;

    GLint loc_lightPosition[MAX_LIGHTS];
    GLint loc_lightDiffuse[MAX_LIGHTS];
    GLint loc_lightAmbient[MAX_LIGHTS];
    GLint loc_lightSpecular[MAX_LIGHTS];
    GLint loc_lightConstant[MAX_LIGHTS];
    GLint loc_lightLinear[MAX_LIGHTS];
    GLint loc_lightQuadratic[MAX_LIGHTS];
};

#endif // ENGINE_PBRSHADER_H

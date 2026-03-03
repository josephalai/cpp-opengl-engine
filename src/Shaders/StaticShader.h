//
// Created by Joseph Alai on 6/30/21.
//

#ifndef ENGINE_STATICSHADER_H
#define ENGINE_STATICSHADER_H
#include "../Entities/CameraInput.h"
#include "../Textures/ModelTexture.h"
#include "ShaderProgram.h"
#include "../Entities/Light.h"


class StaticShader : public ShaderProgram {
private:

    constexpr static const char *VertexPath = "/src/Shaders/Static/VertexShader.glsl";
    constexpr static const char *FragmentPath = "/src/Shaders/Static/FragmentShader.glsl";

    static const int MAX_LIGHTS = 4;
    // attribute names
    const std::string position = "position";
    const std::string texture = "textureCoords";
    const std::string normal = "normal";

    // uniform names
    const std::string transformationMatrix = "transformationMatrix";
    const std::string projectionMatrix = "projectionMatrix";
    const std::string viewMatrix = "viewMatrix";
    const std::string lightColor = "lightColor";
    const std::string useFakeLighting = "useFakeLighting";
    const std::string skyColor = "skyColor";
    const std::string viewPosition = "viewPosition";

    const std::string textureNumberOfRows = "numberOfRows";
    const std::string textureOffset = "offset";

    const std::string light = "light";
    const std::string lightAmbient = "ambient";
    const std::string lightDiffuse = "diffuse";
    const std::string lightSpecular = "specular";
    const std::string lightPosition = "position";
    const std::string lightConstant = "constant";
    const std::string lightLinear = "linear";
    const std::string lightQuadratic = "quadratic";

    const std::string materialShininess = "material.shininess";
    const std::string materialReflectivity = "material.reflectivity";

    const std::string fogDensity = "fogDensity";
    const std::string useNormalMap = "useNormalMap";
    const std::string useSpecularMap = "useSpecularMap";
    const std::string normalMapSampler = "normalMapSampler";
    const std::string specularMapSampler = "specularMapSampler";
    const std::string useInstancing = "useInstancing";

    GLint location_transformationMatrix;
    GLint location_projectionMatrix;
    GLint location_viewMatrix;
    GLint location_viewPosition;

    GLint location_useFakeLighting;
    GLint location_skyColor;
    GLint location_textureNumberOfRows;

    GLint location_textureOffset;

    GLint location_lightPosition[MAX_LIGHTS];

    GLint location_lightAmbient[MAX_LIGHTS];
    GLint location_lightDiffuse[MAX_LIGHTS];
    GLint location_lightSpecular[MAX_LIGHTS];

    GLint location_lightConstant[MAX_LIGHTS];
    GLint location_lightLinear[MAX_LIGHTS];
    GLint location_lightQuadratic[MAX_LIGHTS];

    GLint location_materialShininess;
    GLint location_materialReflectivity;

    GLint location_fogDensity;
    GLint location_useNormalMap;
    GLint location_useSpecularMap;
    GLint location_normalMapSampler;
    GLint location_specularMapSampler;
    GLint location_useInstancing;
public:
    GLuint attribute;

    StaticShader();

    void bindAttributes() override ;

    void loadTransformationMatrix(glm::mat4 matrix = glm::mat4(1.0f)) ;

    void loadProjectionMatrix(glm::mat4 matrix = glm::mat4(1.0f)) ;

    void loadViewMatrix(glm::mat4 matrix = glm::mat4(1.0f));

    void loadViewPosition(Camera *camera);

    void loadLight(std::vector<Light *>light);

    void loadMaterial(Material material);

    void loadFogDensity(float density);
    void loadUseNormalMap(bool use);
    void loadUseSpecularMap(bool use);
    void loadNormalMapSampler(int unit);
    void loadSpecularMapSampler(int unit);

    /// Enable/disable per-instance matrix mode (used by EntityRenderer::renderInstanced).
    void loadUseInstancing(bool use);

    void loadFakeLightingVariable(bool useFakeLighting);

    void loadSkyColorVariable(Color skyColor);

    void loadNumberOfRows(int rows);

    void loadOffset(float x, float y);


protected:
    void getAllUniformLocations() override;

};

#endif //ENGINE_STATICSHADER_H

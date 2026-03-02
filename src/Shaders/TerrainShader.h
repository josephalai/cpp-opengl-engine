//
// Created by Joseph Alai on 7/4/21.
//

#ifndef ENGINE_TERRAINSHADER_H
#define ENGINE_TERRAINSHADER_H

#include "../Entities/CameraInput.h"
#include "../Textures/ModelTexture.h"
#include "ShaderProgram.h"
#include "../Entities/Light.h"
#include "../Textures/TerrainTexturePack.h"


class TerrainShader : public ShaderProgram {

    constexpr static const char *VertexPath = "/src/Shaders/Terrain/VertexShader.glsl";
    constexpr static const char *FragmentPath = "/src/Shaders/Terrain/FragmentShader.glsl";

    static const int MAX_LIGHTS = 4;
// attribute names
    const std::string position = "position";
    const std::string texture = "textureCoords";
    const std::string normal = "normal";

    // uniform names
    const std::string transformationMatrix = "transformationMatrix";
    const std::string projectionMatrix = "projectionMatrix";
    const std::string viewMatrix = "viewMatrix";

    const std::string skyColor = "skyColor";
    const std::string viewPosition = "viewPosition";

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

    const std::string backgroundTexture = "backgroundTexture";
    const std::string rTexture = "rTexture";
    const std::string gTexture = "gTexture";
    const std::string bTexture = "bTexture";
    const std::string blendMap = "blendMap";

    const std::string fogDensity = "fogDensity";

    GLint location_transformationMatrix;
    GLint location_projectionMatrix;
    GLint location_viewMatrix;
    GLint location_skyColor;
    GLint location_viewPosition;

    GLint location_lightPosition[MAX_LIGHTS];

    GLint location_lightAmbient[MAX_LIGHTS];
    GLint location_lightDiffuse[MAX_LIGHTS];
    GLint location_lightSpecular[MAX_LIGHTS];

    GLint location_lightConstant[MAX_LIGHTS];
    GLint location_lightLinear[MAX_LIGHTS];
    GLint location_lightQuadratic[MAX_LIGHTS];

    GLint location_materialShininess;
    GLint location_materialReflectivity;

    GLint location_backgroundTexture;
    GLint location_rTexture;
    GLint location_gTexture;
    GLint location_bTexture;
    GLint location_blendMap;
    GLint location_fogDensity;
public:
    GLuint attribute;

    TerrainShader();

    void bindAttributes() override;

    void loadTransformationMatrix(glm::mat4 matrix = glm::mat4(1.0f));

    void loadProjectionMatrix(glm::mat4 matrix = glm::mat4(1.0f));

    void loadViewMatrix(glm::mat4 matrix = glm::mat4(1.0f));

    void loadLight(std::vector<Light *> lights);

    void loadMaterial(Material material);

    void loadSkyColorVariable(Color skyColor);

    void loadViewPosition(Camera *camera);

    void connectTextureUnits();

    void loadFogDensity(float density);

protected:
    void getAllUniformLocations() override;
};

#endif //ENGINE_TERRAINSHADER_H

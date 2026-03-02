// PBRShader.cpp

#include "PBRShader.h"
#include "../Util/Utils.h"

PBRShader::PBRShader() : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    initialize();
}

void PBRShader::bindAttributes() {
    bindAttribute(0, "position");
    bindAttribute(1, "textureCoords");
    bindAttribute(2, "normal");
    bindAttribute(3, "tangent");
}

void PBRShader::getAllUniformLocations() {
    loc_transformationMatrix = getUniformLocation("transformationMatrix");
    loc_viewMatrix           = getUniformLocation("viewMatrix");
    loc_projectionMatrix     = getUniformLocation("projectionMatrix");
    loc_viewPosition         = getUniformLocation("viewPosition");
    loc_skyColor             = getUniformLocation("skyColor");
    loc_fogDensity           = getUniformLocation("fogDensity");

    loc_albedoValue    = getUniformLocation("albedoValue");
    loc_metallicValue  = getUniformLocation("metallicValue");
    loc_roughnessValue = getUniformLocation("roughnessValue");
    loc_aoValue        = getUniformLocation("aoValue");
    loc_useAlbedoMap    = getUniformLocation("useAlbedoMap");
    loc_useNormalMap    = getUniformLocation("useNormalMap");
    loc_useMetallicMap  = getUniformLocation("useMetallicMap");
    loc_useRoughnessMap = getUniformLocation("useRoughnessMap");
    loc_useAOMap        = getUniformLocation("useAOMap");

    loc_albedoMap    = getUniformLocation("albedoMap");
    loc_normalMap    = getUniformLocation("normalMap");
    loc_metallicMap  = getUniformLocation("metallicMap");
    loc_roughnessMap = getUniformLocation("roughnessMap");
    loc_aoMap        = getUniformLocation("aoMap");

    const std::string lightArr = "light";
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        loc_lightPosition[i]  = getUniformLocation(Utils::shaderArray(lightArr, i, "position"));
        loc_lightDiffuse[i]   = getUniformLocation(Utils::shaderArray(lightArr, i, "diffuse"));
        loc_lightAmbient[i]   = getUniformLocation(Utils::shaderArray(lightArr, i, "ambient"));
        loc_lightSpecular[i]  = getUniformLocation(Utils::shaderArray(lightArr, i, "specular"));
        loc_lightConstant[i]  = getUniformLocation(Utils::shaderArray(lightArr, i, "constant"));
        loc_lightLinear[i]    = getUniformLocation(Utils::shaderArray(lightArr, i, "linear"));
        loc_lightQuadratic[i] = getUniformLocation(Utils::shaderArray(lightArr, i, "quadratic"));
    }
}

void PBRShader::loadTransformationMatrix(const glm::mat4& m) { setMat4(loc_transformationMatrix, m); }
void PBRShader::loadViewMatrix(const glm::mat4& m)           { setMat4(loc_viewMatrix, m); }
void PBRShader::loadProjectionMatrix(const glm::mat4& m)     { setMat4(loc_projectionMatrix, m); }
void PBRShader::loadViewPosition(Camera* camera)             { setVec3(loc_viewPosition, camera->Position); }
void PBRShader::loadSkyColor(Color& c)                       { setVec3(loc_skyColor, c.getColorRGB()); }
void PBRShader::loadFogDensity(float density)                { setFloat(loc_fogDensity, density); }
void PBRShader::loadAlbedoValue(const glm::vec3& v)          { setVec3(loc_albedoValue, v); }
void PBRShader::loadMetallicValue(float v)                   { setFloat(loc_metallicValue, v); }
void PBRShader::loadRoughnessValue(float v)                  { setFloat(loc_roughnessValue, v); }
void PBRShader::loadAOValue(float v)                         { setFloat(loc_aoValue, v); }
void PBRShader::loadUseAlbedoMap(bool v)                     { setBool(loc_useAlbedoMap, v); }
void PBRShader::loadUseNormalMap(bool v)                     { setBool(loc_useNormalMap, v); }
void PBRShader::loadUseMetallicMap(bool v)                   { setBool(loc_useMetallicMap, v); }
void PBRShader::loadUseRoughnessMap(bool v)                  { setBool(loc_useRoughnessMap, v); }
void PBRShader::loadUseAOMap(bool v)                         { setBool(loc_useAOMap, v); }

void PBRShader::connectTextureUnits() {
    setInt(loc_albedoMap,    0);
    setInt(loc_normalMap,    1);
    setInt(loc_metallicMap,  2);
    setInt(loc_roughnessMap, 3);
    setInt(loc_aoMap,        4);
}

void PBRShader::loadLight(const std::vector<Light*>& lights) {
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i < static_cast<int>(lights.size())) {
            setVec3 (loc_lightPosition[i],  lights[i]->getPosition());
            setVec3 (loc_lightDiffuse[i],   lights[i]->getLighting().diffuse);
            setVec3 (loc_lightAmbient[i],   lights[i]->getLighting().ambient);
            setVec3 (loc_lightSpecular[i],  lights[i]->getColor());
            setFloat(loc_lightConstant[i],  lights[i]->getLighting().constant);
            setFloat(loc_lightLinear[i],    lights[i]->getLighting().linear);
            setFloat(loc_lightQuadratic[i], lights[i]->getLighting().quadratic);
        } else {
            setVec3 (loc_lightPosition[i],  glm::vec3(0.0f));
            setVec3 (loc_lightDiffuse[i],   glm::vec3(0.0f));
            setVec3 (loc_lightAmbient[i],   glm::vec3(0.0f));
            setVec3 (loc_lightSpecular[i],  glm::vec3(0.0f));
            setFloat(loc_lightConstant[i],  0.0f);
            setFloat(loc_lightLinear[i],    0.0f);
            setFloat(loc_lightQuadratic[i], 0.0f);
        }
    }
}

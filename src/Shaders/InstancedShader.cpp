// src/Shaders/InstancedShader.cpp

#include "InstancedShader.h"
#include "../Util/Utils.h"

InstancedShader::InstancedShader()
    : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    initialize();
}

void InstancedShader::bindAttributes() {
    bindAttribute(0, "position");
    bindAttribute(1, "textureCoords");
    bindAttribute(2, "normal");
    // Instance matrix columns 3-6 are bound in InstancedModel::setupVAO()
    bindAttribute(3, "instanceMatrix0");
    bindAttribute(4, "instanceMatrix1");
    bindAttribute(5, "instanceMatrix2");
    bindAttribute(6, "instanceMatrix3");
}

void InstancedShader::getAllUniformLocations() {
    loc_viewMatrix           = getUniformLocation("viewMatrix");
    loc_projectionMatrix     = getUniformLocation("projectionMatrix");
    loc_viewPosition         = getUniformLocation("viewPosition");
    loc_skyColor             = getUniformLocation("skyColor");
    loc_textureSampler       = getUniformLocation("textureSampler");
    loc_materialShininess    = getUniformLocation("material.shininess");
    loc_materialReflectivity = getUniformLocation("material.reflectivity");

    const std::string lightArr = "light";
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        loc_lightPosition [i] = getUniformLocation(Utils::shaderArray(lightArr, i, "position"));
        loc_lightDiffuse  [i] = getUniformLocation(Utils::shaderArray(lightArr, i, "diffuse"));
        loc_lightAmbient  [i] = getUniformLocation(Utils::shaderArray(lightArr, i, "ambient"));
        loc_lightSpecular [i] = getUniformLocation(Utils::shaderArray(lightArr, i, "specular"));
        loc_lightConstant [i] = getUniformLocation(Utils::shaderArray(lightArr, i, "constant"));
        loc_lightLinear   [i] = getUniformLocation(Utils::shaderArray(lightArr, i, "linear"));
        loc_lightQuadratic[i] = getUniformLocation(Utils::shaderArray(lightArr, i, "quadratic"));
    }
}

void InstancedShader::loadViewMatrix(const glm::mat4& m)       { setMat4(loc_viewMatrix, m); }
void InstancedShader::loadProjectionMatrix(const glm::mat4& m) { setMat4(loc_projectionMatrix, m); }
void InstancedShader::loadViewPosition(Camera* c)              { setVec3(loc_viewPosition, c->Position); }
void InstancedShader::loadSkyColor(const glm::vec3& c)         { setVec3(loc_skyColor, c); }
void InstancedShader::connectTexture()                          { setInt(loc_textureSampler, 0); }

void InstancedShader::loadLight(const std::vector<Light*>& lights) {
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i < static_cast<int>(lights.size())) {
            setVec3 (loc_lightPosition [i], lights[i]->getPosition());
            setVec3 (loc_lightDiffuse  [i], lights[i]->getLighting().diffuse);
            setVec3 (loc_lightAmbient  [i], lights[i]->getLighting().ambient);
            setVec3 (loc_lightSpecular [i], lights[i]->getColor());
            setFloat(loc_lightConstant [i], lights[i]->getLighting().constant);
            setFloat(loc_lightLinear   [i], lights[i]->getLighting().linear);
            setFloat(loc_lightQuadratic[i], lights[i]->getLighting().quadratic);
        } else {
            setVec3 (loc_lightPosition [i], glm::vec3(0.0f));
            setVec3 (loc_lightDiffuse  [i], glm::vec3(0.0f));
            setVec3 (loc_lightAmbient  [i], glm::vec3(0.0f));
            setVec3 (loc_lightSpecular [i], glm::vec3(0.0f));
            setFloat(loc_lightConstant [i], 1.0f);
            setFloat(loc_lightLinear   [i], 0.0f);
            setFloat(loc_lightQuadratic[i], 0.0f);
        }
    }
}

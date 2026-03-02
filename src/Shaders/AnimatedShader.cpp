// src/Shaders/AnimatedShader.cpp

#include "AnimatedShader.h"
#include "../Util/Utils.h"

AnimatedShader::AnimatedShader()
    : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    initialize();
}

void AnimatedShader::bindAttributes() {
    bindAttribute(0, "position");
    bindAttribute(1, "normal");
    bindAttribute(2, "textureCoords");
    bindAttribute(3, "boneIDs");
    bindAttribute(4, "boneWeights");
}

void AnimatedShader::getAllUniformLocations() {
    loc_transformationMatrix = getUniformLocation("transformationMatrix");
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

    for (int i = 0; i < MAX_BONES; ++i) {
        loc_boneMatrices[i] = getUniformLocation(
            "boneMatrices[" + std::to_string(i) + "]");
    }
}

void AnimatedShader::loadTransformationMatrix(const glm::mat4& m) {
    setMat4(loc_transformationMatrix, m);
}
void AnimatedShader::loadViewMatrix(const glm::mat4& m) {
    setMat4(loc_viewMatrix, m);
}
void AnimatedShader::loadProjectionMatrix(const glm::mat4& m) {
    setMat4(loc_projectionMatrix, m);
}
void AnimatedShader::loadViewPosition(Camera* camera) {
    setVec3(loc_viewPosition, camera->Position);
}
void AnimatedShader::loadSkyColor(const glm::vec3& c) {
    setVec3(loc_skyColor, c);
}

void AnimatedShader::loadLight(const std::vector<Light*>& lights) {
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

void AnimatedShader::loadBoneMatrices(const std::vector<glm::mat4>& matrices) {
    int count = std::min(static_cast<int>(matrices.size()), MAX_BONES);
    for (int i = 0; i < count; ++i) {
        setMat4(loc_boneMatrices[i], matrices[i]);
    }
}

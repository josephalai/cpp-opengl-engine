// ShadowShader.cpp

#include "ShadowShader.h"

ShadowShader::ShadowShader() : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    initialize();
}

void ShadowShader::bindAttributes() {
    bindAttribute(0, "position");
}

void ShadowShader::getAllUniformLocations() {
    loc_lightSpaceMatrix    = getUniformLocation("lightSpaceMatrix");
    loc_transformationMatrix = getUniformLocation("transformationMatrix");
}

void ShadowShader::loadLightSpaceMatrix(const glm::mat4& m) {
    setMat4(loc_lightSpaceMatrix, m);
}

void ShadowShader::loadTransformationMatrix(const glm::mat4& m) {
    setMat4(loc_transformationMatrix, m);
}

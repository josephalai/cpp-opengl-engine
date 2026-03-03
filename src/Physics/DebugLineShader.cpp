//
// DebugLineShader.cpp
//

#include "DebugLineShader.h"

DebugLineShader::DebugLineShader()
    : ShaderProgram(VertexPath, FragmentPath) {
    initialize();
}

void DebugLineShader::bindAttributes() {
    bindAttribute(0, positionAttr);
    bindAttribute(1, colorAttr);
}

void DebugLineShader::getAllUniformLocations() {
    location_projectionMatrix = getUniformLocation("projectionMatrix");
    location_viewMatrix       = getUniformLocation("viewMatrix");
}

void DebugLineShader::loadProjectionMatrix(const glm::mat4& m) {
    setMat4(location_projectionMatrix, m);
}

void DebugLineShader::loadViewMatrix(const glm::mat4& m) {
    setMat4(location_viewMatrix, m);
}

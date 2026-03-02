//
// Created by Joseph Alai on 3/1/22.
//

#include "RectShader.h"

RectShader::RectShader() : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    this->initialize();
    this->loadTransformationMatrix();
    this->loadColor();
    this->loadAlpha();
}

void RectShader::bindAttributes() {
    this->bindAttribute(0, position);
}

void RectShader::loadTransformationMatrix(glm::mat4 matrix) {
    this->setMat4(location_transformationMatrix, matrix);
}

void RectShader::loadColor(glm::vec3 color) {
    this->setVec3(location_color, color);
}

void RectShader::loadAlpha(float alpha) {
    this->setFloat(location_alpha, alpha);
}

void RectShader::getAllUniformLocations() {
    location_transformationMatrix = getUniformLocation(transformationMatrix);
    location_alpha = getUniformLocation(alpha);
    location_color = getUniformLocation(color);
}

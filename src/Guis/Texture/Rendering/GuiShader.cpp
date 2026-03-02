//
// Created by Joseph Alai on 7/18/21.
//

#include "GuiShader.h"

GuiShader::GuiShader() : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    this->initialize();
    this->loadTransformationMatrix();
}

void GuiShader::bindAttributes() {
    this->bindAttribute(0, position);
}

void GuiShader::loadTransformationMatrix(glm::mat4 matrix) {
    this->setMat4(location_transformationMatrix, matrix);
}

void GuiShader::getAllUniformLocations() {
    location_transformationMatrix = getUniformLocation(transformationMatrix);
}

// WaterShader.cpp

#include "WaterShader.h"

WaterShader::WaterShader() : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    initialize();
}

void WaterShader::bindAttributes() {
    bindAttribute(0, "position");
}

void WaterShader::getAllUniformLocations() {
    loc_projectionMatrix     = getUniformLocation("projectionMatrix");
    loc_viewMatrix           = getUniformLocation("viewMatrix");
    loc_transformationMatrix = getUniformLocation("transformationMatrix");
    loc_cameraPosition       = getUniformLocation("cameraPosition");
    loc_lightPosition        = getUniformLocation("lightPosition");
    loc_lightColor           = getUniformLocation("lightColor");
    loc_moveFactor           = getUniformLocation("moveFactor");
    loc_reflectionTexture    = getUniformLocation("reflectionTexture");
    loc_refractionTexture    = getUniformLocation("refractionTexture");
    loc_dudvMap              = getUniformLocation("dudvMap");
    loc_normalMap            = getUniformLocation("normalMap");
    loc_depthMap             = getUniformLocation("depthMap");
}

void WaterShader::loadProjectionMatrix(const glm::mat4& m)     { setMat4(loc_projectionMatrix, m); }
void WaterShader::loadViewMatrix(const glm::mat4& m)           { setMat4(loc_viewMatrix, m); }
void WaterShader::loadTransformationMatrix(const glm::mat4& m) { setMat4(loc_transformationMatrix, m); }
void WaterShader::loadCameraPosition(const glm::vec3& pos)     { setVec3(loc_cameraPosition, pos); }
void WaterShader::loadLightPosition(const glm::vec3& pos)      { setVec3(loc_lightPosition, pos); }
void WaterShader::loadLightColor(const glm::vec3& color)       { setVec3(loc_lightColor, color); }
void WaterShader::loadMoveFactor(float factor)                  { setFloat(loc_moveFactor, factor); }

void WaterShader::connectTextureUnits() {
    setInt(loc_reflectionTexture, 0);
    setInt(loc_refractionTexture, 1);
    setInt(loc_dudvMap,           2);
    setInt(loc_normalMap,         3);
    setInt(loc_depthMap,          4);
}

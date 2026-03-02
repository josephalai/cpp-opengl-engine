//
// Created by Joseph Alai on 7/6/21.
//

#include "TerrainShader.h"
#include "../Util/Utils.h"

TerrainShader::TerrainShader() : ShaderProgram(VertexPath, FragmentPath, nullptr) {
    this->initialize();
    this->loadTransformationMatrix();
    this->start();
    this->setFloat(getUniformLocation("fogDensity"), 0.007f);
    this->stop();
}

void TerrainShader::bindAttributes() {
    this->bindAttribute(0, position);
    this->bindAttribute(1, texture);
    this->bindAttribute(2, normal);
}

void TerrainShader::loadTransformationMatrix(glm::mat4 matrix) {
    this->setMat4(location_transformationMatrix, matrix);
}

void TerrainShader::loadProjectionMatrix(glm::mat4 matrix) {
    this->setMat4(location_projectionMatrix, matrix);
}

void TerrainShader::loadViewMatrix(glm::mat4 matrix) {
    this->setMat4(location_viewMatrix, matrix);
}

void TerrainShader::loadLight(std::vector<Light *> lights) {

    // for textures and lighting
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i < lights.size()) {
            this->setFloat(location_lightConstant[i], lights[i]->getLighting().constant);
            this->setFloat(location_lightLinear[i], lights[i]->getLighting().linear);
            this->setFloat(location_lightQuadratic[i], lights[i]->getLighting().quadratic);

            this->setVec3(location_lightAmbient[i], lights[i]->getLighting().ambient);
            this->setVec3(location_lightDiffuse[i], lights[i]->getLighting().diffuse);
            this->setVec3(location_lightSpecular[i], lights[i]->getColor());
            this->setVec3(location_lightPosition[i], lights[i]->getPosition());
        } else {
            this->setFloat(location_lightConstant[i], 0.0f);
            this->setFloat(location_lightLinear[i], 0.0f);
            this->setFloat(location_lightQuadratic[i], 0.0f);

            this->setVec3(location_lightAmbient[i], glm::vec3(0.0f));
            this->setVec3(location_lightDiffuse[i], glm::vec3(0.0f));
            this->setVec3(location_lightSpecular[i], glm::vec3(0.0f));
            this->setVec3(location_lightPosition[i], glm::vec3(0.0f));
        }
    }
}

void TerrainShader::loadMaterial(Material material) {
    this->setFloat(location_materialShininess, material.shininess);
    this->setFloat(location_materialReflectivity, material.reflectivity);
}

void TerrainShader::loadSkyColorVariable(Color skyColor) {
    this->setVec3(location_skyColor, skyColor.getColorRGB());
}

void TerrainShader::loadViewPosition(Camera *camera) {
    this->setVec3(location_viewPosition, camera->Position);
}

void TerrainShader::connectTextureUnits() {
    this->setInt(location_backgroundTexture, 0);
    this->setInt(location_rTexture, 1);
    this->setInt(location_gTexture, 2);
    this->setInt(location_bTexture, 3);
    this->setInt(location_blendMap, 4);
}

void TerrainShader::loadFogDensity(float density) {
    this->setFloat(location_fogDensity, density);
}
    location_transformationMatrix = getUniformLocation(transformationMatrix);
    location_projectionMatrix = getUniformLocation(projectionMatrix);
    location_viewMatrix = getUniformLocation(viewMatrix);
    location_skyColor = getUniformLocation(skyColor);

    location_viewPosition = getUniformLocation(viewPosition);

    // for textures and lighting
    for (int i = 0; i < MAX_LIGHTS; i++) {
        location_lightPosition[i] = getUniformLocation(Utils::shaderArray(light, i, lightPosition));

        location_lightDiffuse[i] = getUniformLocation(Utils::shaderArray(light, i, lightDiffuse));
        location_lightAmbient[i] = getUniformLocation(Utils::shaderArray(light, i, lightAmbient));
        location_lightSpecular[i] = getUniformLocation(Utils::shaderArray(light, i, lightSpecular));

        location_lightConstant[i] = getUniformLocation(Utils::shaderArray(light, i, lightConstant));
        location_lightLinear[i] = getUniformLocation(Utils::shaderArray(light, i, lightLinear));
        location_lightQuadratic[i] = getUniformLocation(Utils::shaderArray(light, i, lightQuadratic));
    }

    location_materialShininess = getUniformLocation(materialShininess);
    location_materialReflectivity = getUniformLocation(materialReflectivity);

    location_backgroundTexture = getUniformLocation(backgroundTexture);
    location_rTexture = getUniformLocation(rTexture);
    location_gTexture = getUniformLocation(gTexture);
    location_bTexture = getUniformLocation(bTexture);
    location_blendMap = getUniformLocation(blendMap);
    location_fogDensity = getUniformLocation(fogDensity);
}
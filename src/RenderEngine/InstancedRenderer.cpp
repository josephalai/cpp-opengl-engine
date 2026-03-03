// src/RenderEngine/InstancedRenderer.cpp

#include "InstancedRenderer.h"

InstancedRenderer::InstancedRenderer(InstancedShader* s) : shader(s) {}

void InstancedRenderer::addInstance(InstancedModel* model, const glm::mat4& transform) {
    batches[model].push_back(transform);
}

void InstancedRenderer::render(const std::vector<Light*>& lights,
                                Camera* camera,
                                const glm::mat4& projectionMatrix,
                                const glm::vec3& skyColor) {
    shader->start();
    shader->loadViewMatrix(camera->getViewMatrix());
    shader->loadProjectionMatrix(projectionMatrix);
    shader->loadViewPosition(camera);
    shader->loadLight(lights);
    shader->loadSkyColor(skyColor);
    shader->connectTexture();

    for (auto& [model, transforms] : batches) {
        if (transforms.empty()) continue;
        drawBatch(model, transforms);
    }

    shader->stop();
    clear();
}

void InstancedRenderer::drawBatch(InstancedModel* model,
                                   const std::vector<glm::mat4>& transforms) {
    // Upload transforms to the instance VBO
    glBindBuffer(GL_ARRAY_BUFFER, model->getInstanceVBO());
    GLsizeiptr needed = static_cast<GLsizeiptr>(transforms.size() * sizeof(glm::mat4));
    glBufferData(GL_ARRAY_BUFFER, needed, transforms.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (model->textureID) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, model->textureID);
    }

    glBindVertexArray(model->vaoID);
    glDrawElementsInstanced(GL_TRIANGLES,
                            model->indexCount,
                            GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(transforms.size()));
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

void InstancedRenderer::clear() {
    for (auto& [model, vec] : batches) {
        vec.clear();
    }
}

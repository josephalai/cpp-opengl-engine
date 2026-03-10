// src/RenderEngine/InstancedRenderer.cpp

#include "InstancedRenderer.h"
#include <algorithm>
#include <iostream>
#include <glm/gtx/norm.hpp>

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

    for (auto& [mdl, transforms] : batches) {
        if (transforms.empty()) continue;

        // Phase 4 Step 4.3 — Discard instances beyond maxViewDist_.
        if (maxViewDist_ > 0.0f && camera) {
            glm::vec3 camPos = camera->getPosition();
            float maxDist2 = maxViewDist_ * maxViewDist_;
            transforms.erase(
                std::remove_if(transforms.begin(), transforms.end(),
                    [&](const glm::mat4& m) {
                        glm::vec3 instancePos(m[3][0], m[3][1], m[3][2]);
                        glm::vec3 diff = instancePos - camPos;
                        return glm::dot(diff, diff) > maxDist2;
                    }),
                transforms.end());
            if (transforms.empty()) continue;
        }

        drawBatch(mdl, transforms);
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
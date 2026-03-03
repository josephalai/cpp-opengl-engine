// src/RenderEngine/AnimatedRenderer.cpp

#include "AnimatedRenderer.h"

AnimatedRenderer::AnimatedRenderer(AnimatedShader* s) : shader(s) {}

void AnimatedRenderer::render(const std::vector<AnimatedEntity*>& entities,
                               float deltaTime,
                               const std::vector<Light*>& lights,
                               Camera* camera,
                               const glm::mat4& projectionMatrix) {
    shader->start();
    shader->loadViewMatrix(camera->getViewMatrix());
    shader->loadProjectionMatrix(projectionMatrix);
    shader->loadViewPosition(camera);
    shader->loadLight(lights);

    for (AnimatedEntity* ae : entities) {
        if (!ae || !ae->model || !ae->controller) continue;

        // Advance animation and get bone matrices
        std::vector<glm::mat4> boneMatrices =
            ae->controller->update(deltaTime, ae->model->skeleton);

        shader->loadBoneMatrices(boneMatrices);

        glm::mat4 transform = Maths::createTransformationMatrix(
            ae->position, ae->rotation, ae->scale)
            * ae->model->coordinateCorrection;
        shader->loadTransformationMatrix(transform);

        for (const AnimatedMesh& mesh : ae->model->meshes) {
            renderMesh(mesh, boneMatrices);
        }
    }

    shader->stop();
}

void AnimatedRenderer::renderMesh(const AnimatedMesh& mesh,
                                   const std::vector<glm::mat4>& /*boneMatrices*/) {
    if (mesh.textureID) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.textureID);
    }

    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(mesh.indices.size()),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
}

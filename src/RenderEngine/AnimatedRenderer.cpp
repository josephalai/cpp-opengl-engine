// src/RenderEngine/AnimatedRenderer.cpp

#include "AnimatedRenderer.h"

AnimatedRenderer::AnimatedRenderer(AnimatedShader* s) : shader(s) {
    // Create a 1×1 white fallback texture so that meshes without an embedded
    // or external texture still render correctly.  Without this the fragment
    // shader's alpha discard (texColor.a < 0.5) would make untextured
    // models invisible — common for Meshy GLBs that only set baseColorFactor.
    unsigned char white[] = { 255, 255, 255, 255 };
    glGenTextures(1, &fallbackTexture_);
    glBindTexture(GL_TEXTURE_2D, fallbackTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

AnimatedRenderer::~AnimatedRenderer() {
    if (fallbackTexture_) {
        glDeleteTextures(1, &fallbackTexture_);
        fallbackTexture_ = 0;
    }
}

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

        // modelRotationMat is the authoritative model-space correction.
        // It defaults to the loader's coordinateCorrection (set in EntityFactory /
        // Engine / SceneLoaderJson), but a prefab's model_rotation field overrides
        // it entirely.  coordinateCorrection is NOT multiplied again here so that
        // an explicit prefab rotation is not compounded with the auto-detected one.
        glm::mat4 transform = Maths::createTransformationMatrix(
            ae->position + ae->modelOffset, ae->rotation, ae->scale)
            * ae->modelRotationMat;
        shader->loadTransformationMatrix(transform);

        for (const AnimatedMesh& mesh : ae->model->meshes) {
            renderMesh(mesh, boneMatrices);
        }
    }

    shader->stop();
}

void AnimatedRenderer::renderMesh(const AnimatedMesh& mesh,
                                   const std::vector<glm::mat4>& /*boneMatrices*/) {
    glActiveTexture(GL_TEXTURE0);
    // Use the mesh's own texture when available, otherwise bind a 1×1 white
    // fallback so the fragment shader's alpha discard doesn't kill the mesh.
    glBindTexture(GL_TEXTURE_2D, mesh.textureID ? mesh.textureID : fallbackTexture_);

    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(mesh.indices.size()),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

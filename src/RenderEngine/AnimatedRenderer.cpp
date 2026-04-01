// src/RenderEngine/AnimatedRenderer.cpp

#include "AnimatedRenderer.h"
#include <iostream>

AnimatedRenderer::AnimatedRenderer(AnimatedShader* s) : shader(s) {
    // Create a 1×1 opaque-white RGBA fallback texture.
    // Used in renderMesh() when a mesh has no embedded texture (textureID==0).
    // Without this, the fragment shader's `if (texColor.a < 0.5) discard`
    // receives alpha=0 from an unbound sampler and discards every fragment,
    // making the model completely invisible.
    glGenTextures(1, &fallbackTextureID_);
    glBindTexture(GL_TEXTURE_2D, fallbackTextureID_);
    const unsigned char white[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "[AnimatedRenderer] Created fallback texture (GL id="
              << fallbackTextureID_ << ").\n";
}

AnimatedRenderer::~AnimatedRenderer() {
    if (fallbackTextureID_) {
        glDeleteTextures(1, &fallbackTextureID_);
        fallbackTextureID_ = 0;
    }
}

void AnimatedRenderer::render(const std::vector<AnimatedEntity*>& entities,
                               float deltaTime,
                               const std::vector<Light*>& lights,
                               Camera* camera,
                               const glm::mat4& projectionMatrix) {
    // Throttled one-time summary log (prints once, then every ~5 seconds only
    // when the entity count changes).
    {
        static size_t lastLoggedCount = ~size_t(0);
        static float  logCooldown     = 0.0f;
        logCooldown -= deltaTime;
        if (entities.size() != lastLoggedCount && logCooldown <= 0.0f) {
            std::cout << "[AnimatedRenderer::render] Rendering "
                      << entities.size() << " animated entity(ies).\n";
            lastLoggedCount = entities.size();
            logCooldown = 5.0f;
        }
    }

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

        // ---- Render meshes ----
        // Modular path: use the pre-built active mesh list (naked + equipment).
        // Legacy path : iterate model->meshes directly.
        if (ae->isModular && !ae->activeMeshes.empty()) {
            for (const AnimatedMesh* mesh : ae->activeMeshes) {
                renderMesh(*mesh, boneMatrices);
            }
        } else {
            for (const AnimatedMesh& mesh : ae->model->meshes) {
                renderMesh(mesh, boneMatrices);
            }
        }
    }

    shader->stop();
}

void AnimatedRenderer::renderMesh(const AnimatedMesh& mesh,
                                   const std::vector<glm::mat4>& /*boneMatrices*/) {
    glActiveTexture(GL_TEXTURE0);
    // Always bind a texture so the fragment shader's alpha-discard sees alpha=1.
    // If the mesh has no embedded texture, bind the 1×1 white fallback so the
    // model renders in flat white rather than being invisible.
    GLuint texToBind = mesh.textureID ? mesh.textureID : fallbackTextureID_;
    glBindTexture(GL_TEXTURE_2D, texToBind);

    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(mesh.indices.size()),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
}

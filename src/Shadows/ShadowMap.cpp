// ShadowMap.cpp

#include "ShadowMap.h"
#include "../Toolbox/Maths.h"

#include <iostream>

ShadowMap::ShadowMap(int size) : size_(size), fbo_(0), depthTexture_(0) {
    init();
}

ShadowMap::~ShadowMap() {
    if (fbo_)          glDeleteFramebuffers(1, &fbo_);
    if (depthTexture_) glDeleteTextures(1, &depthTexture_);
}

void ShadowMap::init() {
    // Create depth texture
    glGenTextures(1, &depthTexture_);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 size_, size_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // Areas outside the shadow map are lit (not in shadow)
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Create FBO
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[ShadowMap] Framebuffer incomplete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::bindForWriting() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, size_, size_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::unbind(int displayWidth, int displayHeight) const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, displayWidth, displayHeight);
}

glm::mat4 ShadowMap::computeLightSpaceMatrix(const glm::vec3& lightDir,
                                              const glm::vec3& viewCenter,
                                              float            radius) const {
    // Build a look-at matrix pointing from the viewCenter along the light direction
    glm::vec3 lightPos = viewCenter - glm::normalize(lightDir) * radius;
    glm::vec3 up       = glm::abs(glm::normalize(lightDir).y) < 0.99f
                             ? glm::vec3(0.0f, 1.0f, 0.0f)
                             : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, viewCenter, up);

    // Orthographic projection that tightly wraps the shadow frustum
    float r = radius;
    glm::mat4 lightProj = glm::ortho(-r, r, -r, r, 0.1f, radius * 2.5f);

    return lightProj * lightView;
}

void ShadowMap::renderShadowMapFromRegistry(entt::registry&    registry,
                                             Entity*             player,
                                             const glm::mat4&   lightSpaceMatrix,
                                             ShadowShader*       shader) const {
    bindForWriting();

    shader->start();
    shader->loadLightSpaceMatrix(lightSpaceMatrix);

    // Render Player shadow
    if (player && player->getModel()) {
        glm::mat4 t = Maths::createTransformationMatrix(
            player->getPosition(), player->getRotation(), player->getScale());
        shader->loadTransformationMatrix(t);
        RawModel* raw = player->getModel()->getRawModel();
        glBindVertexArray(raw->getVaoId());
        glEnableVertexAttribArray(0);
        glDrawElements(GL_TRIANGLES, raw->getVertexCount(), GL_UNSIGNED_INT, 0);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // Render StaticModelComponent entity shadows
    auto staticView = registry.view<StaticModelComponent, TransformComponent>();
    for (auto e : staticView) {
        const auto& smc = staticView.get<StaticModelComponent>(e);
        const auto& tc  = staticView.get<TransformComponent>(e);
        if (!smc.model) continue;
        glm::mat4 t = Maths::createTransformationMatrix(tc.position, tc.rotation, tc.scale);
        shader->loadTransformationMatrix(t);
        RawModel* raw = smc.model->getRawModel();
        glBindVertexArray(raw->getVaoId());
        glEnableVertexAttribArray(0);
        glDrawElements(GL_TRIANGLES, raw->getVertexCount(), GL_UNSIGNED_INT, 0);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    shader->stop();
}
                                 const glm::mat4&            lightSpaceMatrix,
                                 ShadowShader*               shader) const {
    bindForWriting();

    shader->start();
    shader->loadLightSpaceMatrix(lightSpaceMatrix);

    // Render each entity using position-only attributes
    for (Entity* e : entities) {
        glm::mat4 transform = Maths::createTransformationMatrix(
            e->getPosition(), e->getRotation(), e->getScale());
        shader->loadTransformationMatrix(transform);

        RawModel* raw = e->getModel()->getRawModel();
        glBindVertexArray(raw->getVaoId());
        glEnableVertexAttribArray(0);
        glDrawElements(GL_TRIANGLES, raw->getVertexCount(), GL_UNSIGNED_INT, 0);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    shader->stop();
}

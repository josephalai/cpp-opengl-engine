//
// Created by Joseph Alai on 7/6/21.
//

#include "EntityRenderer.h"
#include "RenderStyle.h"
#include "../Toolbox/Maths.h"

EntityRenderer::EntityRenderer(StaticShader *shader) {
    this->shader = shader;
}

EntityRenderer::~EntityRenderer() {
    if (instanceVBO_) {
        glDeleteBuffers(1, &instanceVBO_);
        instanceVBO_ = 0;
    }
}

/**
 * @brief accepts a map[model]std::vector<Entity *>, and traverses through
 *        it, and draws them -- so as not to copy objects.
 * @param entities
 */
void EntityRenderer::render(std::map<TexturedModel *, std::vector<Entity *>> *entities) {
    std::map<TexturedModel *, std::vector<Entity *>>::iterator it = entities->begin();
    TexturedModel *model;
    while (it != entities->end()) {
        model = it->first;
        prepareTexturedModel(model);
        std::vector<Entity *> batch = entities->find(model)->second;
        batch = entities->find(model)->second;
        for (Entity *entity : batch) {
            prepareInstance(entity);
            // draw elements
            glDrawElements(GL_TRIANGLES, model->getRawModel()->getVertexCount(), GL_UNSIGNED_INT, 0);
        }
        unbindTexturedModel();
        it++;
    }
}

/**
 * @brief Instanced rendering: uploads per-instance model matrices as vertex
 *        attributes (locations 4–7) and issues glDrawElementsInstanced.
 */
void EntityRenderer::renderInstanced(TexturedModel* model, const std::vector<Entity*>& batch) {
    if (batch.empty()) return;

    // Build a flat array of mat4 transforms for all instances
    std::vector<glm::mat4> matrices;
    matrices.reserve(batch.size());
    for (Entity* e : batch) {
        matrices.push_back(
            Maths::createTransformationMatrix(e->getPosition(), e->getRotation(), e->getScale()));
    }

    prepareTexturedModel(model);
    shader->loadUseInstancing(true);

    GLuint vbo = getOrCreateInstanceVBO();
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    GLsizeiptr needed = static_cast<GLsizeiptr>(matrices.size() * sizeof(glm::mat4));
    if (needed > instanceVBOCap_) {
        // Grow: allocate a new buffer large enough for this batch
        glBufferData(GL_ARRAY_BUFFER, needed, matrices.data(), GL_DYNAMIC_DRAW);
        instanceVBOCap_ = needed;
    } else {
        // Reuse existing allocation — avoids GPU-side reallocation cost
        glBufferSubData(GL_ARRAY_BUFFER, 0, needed, matrices.data());
    }

    // A mat4 occupies 4 consecutive vec4 attribute slots (locations 4–7)
    for (int col = 0; col < 4; ++col) {
        GLuint loc = static_cast<GLuint>(4 + col);
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(glm::mat4),
                              reinterpret_cast<const void*>(col * sizeof(glm::vec4)));
        glVertexAttribDivisor(loc, 1);  // advance once per instance, not per vertex
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawElementsInstanced(GL_TRIANGLES,
                            model->getRawModel()->getVertexCount(),
                            GL_UNSIGNED_INT,
                            nullptr,
                            static_cast<GLsizei>(batch.size()));

    // Reset instance divisors so regular rendering still works
    for (int col = 0; col < 4; ++col) {
        GLuint loc = static_cast<GLuint>(4 + col);
        glVertexAttribDivisor(loc, 0);
        glDisableVertexAttribArray(loc);
    }

    shader->loadUseInstancing(false);
    unbindTexturedModel();
}

/**
 * @brief binds the attribute arrays of the model. disables
 *        or enables culling based on the transparency of the texture,
 *        loads the shine variables, and binds the texture.
 * @param model
 */
void EntityRenderer::prepareTexturedModel(TexturedModel *model) {
    RawModel *rawModel = model->getRawModel();

    // bind the current vao
    glBindVertexArray(rawModel->getVaoId());

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    ModelTexture *texture = model->getModelTexture();

    shader->loadNumberOfRows(texture->getNumberOfRows());

    if (texture->isHasTransparency()) {
        RenderStyle::disableCulling();
    }
    shader->loadFakeLightingVariable(texture->isUseFakeLighting());
    glActiveTexture(GL_TEXTURE0);
    // bind texture
    glBindTexture(GL_TEXTURE_2D, model->getModelTexture()->getId());
}

/**
 * @brief unbinds the texture model after it's use.
 */
void EntityRenderer::unbindTexturedModel() {
    RenderStyle::enableCulling();
    // clean up
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindVertexArray(0);
}

/**
 * @brief sets the initial transformation (view) matrix.
 * @param entity
 */
void EntityRenderer::prepareInstance(Entity *entity) {
    // creates the matrices to be passed into the shader
    glm::mat4 transformationMatrix = Maths::createTransformationMatrix(entity->getPosition(), entity->getRotation(),
                                                                       entity->getScale());
    shader->loadTransformationMatrix(transformationMatrix);
    shader->loadMaterial(entity->getMaterial());
    shader->loadOffset(entity->getTextureXOffset(), entity->getTextureYOffset());
}

GLuint EntityRenderer::getOrCreateInstanceVBO() {
    if (instanceVBO_ == 0) {
        glGenBuffers(1, &instanceVBO_);
    }
    return instanceVBO_;
}
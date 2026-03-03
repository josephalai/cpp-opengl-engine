//
// Created by Joseph Alai on 6/30/21.
//

#ifndef ENGINE_ENTITYRENDERER_H
#define ENGINE_ENTITYRENDERER_H
#include <map>
#include <vector>

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "../Models/TexturedModel.h"
#include "../Shaders/StaticShader.h"
#include "../Entities/Entity.h"

class EntityRenderer {
private:
    StaticShader *shader;
    GLuint instanceVBO_ = 0; ///< Per-frame dynamic VBO for instance matrices.

public:

    EntityRenderer(StaticShader *shader);
    ~EntityRenderer();

    /**
     * @brief accepts a map[model]std::vector<Entity *>, and traverses through
     *        it, and draws them -- so as not to copy objects.
     * @param entities
     */
    void render(std::map<TexturedModel *, std::vector<Entity *>> *entities);

    /**
     * @brief Instanced rendering: uploads per-instance model matrices as vertex
     *        attributes and issues a single glDrawElementsInstanced call for all
     *        entities in the batch. All entities must share the same TexturedModel.
     *
     * Instance matrices are supplied as attributes at locations 4–7 (4 vec4 = mat4)
     * with an attribute divisor of 1, leaving locations 0–3 for per-vertex data.
     * The shader must be started and per-frame uniforms loaded before this call.
     *
     * @param model  The shared model (VAO/texture).
     * @param batch  Entities to render — all should use @p model.
     */
    void renderInstanced(TexturedModel* model, const std::vector<Entity*>& batch);

private:
    /**
     * @brief binds the attribute arrays of the model. disables
     *        or enables culling based on the transparency of the texture,
     *        loads the shine variables, and binds the texture.
     * @param model
     */
    void prepareTexturedModel(TexturedModel *model);

    /**
     * @brief unbinds the texture model after it's use.
     */
    void unbindTexturedModel();

    /**
     * @brief sets the initial transformation (view) matrix.
     * @param entity
     */
    void prepareInstance(Entity *entity);

    /// Ensure the instance VBO exists and return it.
    GLuint getOrCreateInstanceVBO();
};

#endif //ENGINE_ENTITYRENDERER_H

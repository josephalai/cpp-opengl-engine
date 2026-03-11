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
#include <glm/glm.hpp>

#include "../Models/TexturedModel.h"
#include "../Shaders/StaticShader.h"
#include "../Entities/Entity.h"
#include "../Textures/ModelTexture.h"  // for Material

class EntityRenderer {
private:
    StaticShader *shader;
    GLuint instanceVBO_         = 0; ///< Per-frame dynamic VBO for instance matrices.
    GLsizeiptr instanceVBOCap_  = 0; ///< Current allocated capacity of instanceVBO_ in bytes.

public:

    /// Lightweight render data for ECS-sourced static entities.
    /// Replaces Entity* for the batched render path.
    struct RenderData {
        glm::vec3 position;
        glm::vec3 rotation;
        float     scale      = 1.0f;
        float     texXOffset = 0.0f;
        float     texYOffset = 0.0f;
        Material  material   = {0.1f, 0.9f};
    };

    EntityRenderer(StaticShader *shader);
    ~EntityRenderer();
    /**
     * @brief accepts a map[model]std::vector<Entity *>, and traverses through
     *        it, and draws them -- so as not to copy objects.
     * @param entities
     */
    void render(std::map<TexturedModel *, std::vector<Entity *>> *entities);

    /// ECS path: render static entities from component data (no Entity* needed).
    void renderStaticBatch(std::map<TexturedModel*, std::vector<RenderData>>* batches);

    /**
     * @brief Instanced rendering: uploads per-instance model matrices as vertex
     *        attributes and issues a single glDrawElementsInstanced call for all
     *        entities in the batch. All entities must share the same TexturedModel.
     */
    void renderInstanced(TexturedModel* model, const std::vector<Entity*>& batch);

private:
    void prepareTexturedModel(TexturedModel *model);
    void unbindTexturedModel();
    void prepareInstance(Entity *entity);
    void prepareInstanceData(const RenderData& data);
    GLuint getOrCreateInstanceVBO();
};

#endif //ENGINE_ENTITYRENDERER_H

// src/RenderEngine/InstancedRenderer.h
// Renders many identical objects with a single glDrawElementsInstanced call.

#ifndef ENGINE_INSTANCEDRENDERER_H
#define ENGINE_INSTANCEDRENDERER_H

#include <vector>
#include <map>
#include <glm/glm.hpp>
#include "InstancedModel.h"
#include "../Shaders/InstancedShader.h"
#include "../Entities/Camera.h"
#include "../Entities/Light.h"

class InstancedRenderer {
public:
    explicit InstancedRenderer(InstancedShader* shader);

    /// Queue transform for a model.
    void addInstance(InstancedModel* model, const glm::mat4& transform);

    /// Draw all queued instances and then clear the queues.
    void render(const std::vector<Light*>& lights,
                Camera* camera,
                const glm::mat4& projectionMatrix,
                const glm::vec3& skyColor);

    /// Clear per-frame instance queues (called automatically by render()).
    void clear();

private:
    InstancedShader* shader;
    std::map<InstancedModel*, std::vector<glm::mat4>> batches;

    void drawBatch(InstancedModel* model,
                   const std::vector<glm::mat4>& transforms);
};

#endif // ENGINE_INSTANCEDRENDERER_H

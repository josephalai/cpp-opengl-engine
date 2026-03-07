//
// Created by Joseph Alai on 7/28/21.
//
#include "AssimpEntityRenderer.h"
AssimpEntityRenderer::AssimpEntityRenderer(AssimpStaticShader *shader) {
    this->shader = shader;
}

/**
 * @brief accepts a map[mesh]vector<AssimpModelComponent> and renders each batch.
 * @param scenes  Batched scene components grouped by AssimpMesh*.
 */
void AssimpEntityRenderer::render(std::map<AssimpMesh *, std::vector<AssimpModelComponent>> *scenes) {
    for (const auto& [model, batch] : *scenes) {
        if (!model) continue;
        for (const AssimpModelComponent& comp : batch) {
            prepareInstance(comp);
            model->render(shader);
        }
    }
}


/**
 * @brief uploads the transformation matrix and material for one scene component.
 * @param comp  The pure-data component to render.
 */
void AssimpEntityRenderer::prepareInstance(const AssimpModelComponent& comp) {
    glm::mat4 transformationMatrix = Maths::createTransformationMatrix(
        comp.position, comp.rotation, comp.scale);
    shader->loadTransformationMatrix(transformationMatrix);
    if (comp.mesh) {
        shader->loadMaterial(comp.mesh->getMaterial());
    }
}
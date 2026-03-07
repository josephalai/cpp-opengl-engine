#include "AssimpEntityRenderer.h"
AssimpEntityRenderer::AssimpEntityRenderer(AssimpStaticShader *shader) {
    this->shader = shader;
}

/**
 * @brief accepts a map[model]std::vector<AssimpModelComponent>, and traverses
 *        through it, and draws them -- so as not to copy objects.
 * @param scenes
 */
void AssimpEntityRenderer::render(std::map<AssimpMesh *, std::vector<AssimpModelComponent>> *scenes) {
    auto it = scenes->begin();
    AssimpMesh *model;
    while (it != scenes->end()) {
        model = it->first;
        std::vector<AssimpModelComponent>& batch = it->second;
        for (const AssimpModelComponent& comp : batch) {
            prepareInstance(comp);
            // draw elements
            model->render(shader);
        }
        it++;
    }
}


/**
 * @brief sets the initial transformation (view) matrix.
 * @param comp
 */
void AssimpEntityRenderer::prepareInstance(const AssimpModelComponent& comp) {
    // creates the matrices to be passed into the shader
    glm::mat4 transformationMatrix = Maths::createTransformationMatrix(comp.position, comp.rotation,
                                                                       comp.scale);
    shader->loadTransformationMatrix(transformationMatrix);
    shader->loadMaterial(comp.mesh->getMaterial());
}
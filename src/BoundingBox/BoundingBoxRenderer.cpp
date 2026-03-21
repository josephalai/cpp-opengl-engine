//
// Created by Joseph Alai on 7/25/21.
//

#include "BoundingBoxRenderer.h"
#include "../Toolbox/Maths.h"
#include "../RenderEngine/RenderStyle.h"
#include "../OpenGLWrapper/OpenGLUtils.h"

BoundingBoxRenderer::BoundingBoxRenderer(BoundingBoxShader *shader, glm::mat4 projectionMatrix) :
        shader(shader) {

    shader->start();
    shader->loadProjectionMatrix(projectionMatrix);
    shader->stop();

}

/**
 * @brief Load the bounding boxes as well as the transformations of those boxes, and render them.
 *
 * @param entities
 */
void BoundingBoxRenderer::render(std::map<RawBoundingBox *, std::vector<Entity *>> *entities) {
    auto itEntityMap = entities->begin();
    RawBoundingBox *pRawBox;
    while (itEntityMap != entities->end()) {

        // bind the VAO of the box
        pRawBox = itEntityMap->first;
        if (!pRawBox) { ++itEntityMap; continue; }  // skip null models (e.g. animated-path player)
        prepareRawBoundingBox(pRawBox);

//        std::vector<Entity *> batch = entities->find(pRawBox)->second;
        std::vector<Entity *> batch = itEntityMap->second;
        for (Entity *entity : batch) {
            prepareInstance(entity);

            // draw elements
            auto glDrawType = GL_TRIANGLE_STRIP;
            if(pRawBox->isMesh()) {
                glDrawType = GL_TRIANGLES;
            }
            glDrawElements(glDrawType, pRawBox->getVertexCount(), GL_UNSIGNED_INT, 0);
        }
        unbindBox(pRawBox);
        itEntityMap++;
    }
}

/**
 * @brief Binds the VAO of the box so it can be rendered.
 *
 * @param box
 */
void BoundingBoxRenderer::prepareRawBoundingBox(RawBoundingBox *box) {

    OpenGLUtils::cullBackFaces(false);
    glBindVertexArray(box->getVaoId());
    glEnableVertexAttribArray(0); // position

    if (!box->isMesh()) {
        glEnable(GL_PRIMITIVE_RESTART_INDEX);
        glPrimitiveRestartIndex(8);
    }
}

void BoundingBoxRenderer::unbindBox(RawBoundingBox *box) {
    RenderStyle::enableCulling();
    if (!box->isMesh()) {
        glDisable(GL_PRIMITIVE_RESTART_INDEX);
    }
    OpenGLUtils::cullBackFaces(true);

    // clean up
    glDisableVertexAttribArray(0); // position
    glBindVertexArray(0);
}

void BoundingBoxRenderer::prepareInstance(Entity *entity) {
    shader->loadBoxColor(entity->getBoundingBox()->getBoxColor());
    glm::mat4 transformationMatrix = Maths::createTransformationMatrix(entity->getPosition(), entity->getRotation(),
                                                                       entity->getScale());
    shader->loadTransformationMatrix(transformationMatrix);
}

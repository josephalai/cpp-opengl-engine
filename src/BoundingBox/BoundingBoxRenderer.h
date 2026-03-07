//
// Created by Joseph Alai on 7/25/21.
//

#ifndef ENGINE_BOUNDINGBOXRENDERER_H
#define ENGINE_BOUNDINGBOXRENDERER_H

#include "../Entities/Entity.h"
#include "../Shaders/StaticShader.h"
#include <map>
#include <iostream>
#include <vector>
#include "BoundingBoxShader.h"

class BoundingBoxRenderer {
private:

    BoundingBoxShader *shader;
public:
    explicit BoundingBoxRenderer(BoundingBoxShader *shader, glm::mat4 projectionMatrix);

    void render(std::map<RawBoundingBox *, std::vector<Entity *>> *entities);

private:
    void prepareRawBoundingBox(RawBoundingBox *box);

    /**
     * @brief unbinds the texture model after it's use.
     */
    void unbindBox(RawBoundingBox *box);

    /**
     * @brief sets the initial transformation (view) matrix.
     * @param entity
     */
    void prepareInstance(Entity *entity);
};


#endif //ENGINE_BOUNDINGBOXRENDERER_H

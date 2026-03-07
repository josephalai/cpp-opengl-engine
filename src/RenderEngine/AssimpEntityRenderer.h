//
// Created by Joseph Alai on 7/8/21.
//

#ifndef ENGINE_ASSIMPENTITYRENDERER_H
#define ENGINE_ASSIMPENTITYRENDERER_H
#include "../ECS/Components/AssimpModelComponent.h"
#include "../Toolbox/Maths.h"
#include "AssimpEntityLoader.h"
#include <iostream>
#include <cstdio>
#include <map>
#include <vector>

class AssimpEntityRenderer {
private:
    AssimpStaticShader *shader;

public:
    AssimpEntityRenderer(AssimpStaticShader *shader);

    /**
     * @brief accepts a map[mesh]vector<AssimpModelComponent> and renders each batch.
     * @param scenes  Batched scene components grouped by AssimpMesh*.
     */
    void render(std::map<AssimpMesh *, std::vector<AssimpModelComponent>> *scenes);

    /**
     * @brief uploads the transformation matrix and material for one scene component.
     * @param comp  The pure-data component to render.
     */
    void prepareInstance(const AssimpModelComponent& comp);

};
#endif //ENGINE_ASSIMPENTITYRENDERER_H

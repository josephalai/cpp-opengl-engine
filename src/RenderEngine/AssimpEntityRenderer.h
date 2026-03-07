//
// Created by Joseph Alai on 7/8/21.
//

#ifndef ENGINE_ASSIMPENTITYRENDERER_H
#define ENGINE_ASSIMPENTITYRENDERER_H
#include "../ECS/Components/AssimpModelComponent.h"
#include "AssimpEntityLoader.h"
#include "../Toolbox/Maths.h"
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
     * @brief accepts a map[model]std::vector<AssimpModelComponent>, and traverses
     *        through it, and draws them -- so as not to copy objects.
     * @param scenes
     */
    void render(std::map<AssimpMesh *, std::vector<AssimpModelComponent>> *scenes);

    /**
     * @brief sets the initial transformation (view) matrix.
     * @param comp
     */
    void prepareInstance(const AssimpModelComponent& comp);

};
#endif //ENGINE_ASSIMPENTITYRENDERER_H

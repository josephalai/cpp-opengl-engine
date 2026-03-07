#ifndef ECS_MATERIALDATACOMPONENT_H
#define ECS_MATERIALDATACOMPONENT_H

#include "../../Textures/ModelTexture.h"  // for struct Material

struct MaterialDataComponent {
    Material material  = {0.1f, 0.9f};
    bool     activated = false;
};

#endif // ECS_MATERIALDATACOMPONENT_H

#ifndef ECS_RENDERCOMPONENT_H
#define ECS_RENDERCOMPONENT_H

class TexturedModel;

struct RenderComponent {
    TexturedModel* model        = nullptr;
    int            textureIndex = 0;
};

#endif // ECS_RENDERCOMPONENT_H

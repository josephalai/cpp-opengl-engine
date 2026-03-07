#ifndef ECS_COLLIDERCOMPONENT_H
#define ECS_COLLIDERCOMPONENT_H

class BoundingBox;

struct ColliderComponent {
    BoundingBox* box = nullptr;
};

#endif // ECS_COLLIDERCOMPONENT_H

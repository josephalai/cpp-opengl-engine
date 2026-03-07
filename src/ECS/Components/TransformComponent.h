#ifndef ECS_TRANSFORMCOMPONENT_H
#define ECS_TRANSFORMCOMPONENT_H
#include <glm/glm.hpp>

struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    float     scale    = 1.0f;
};

#endif // ECS_TRANSFORMCOMPONENT_H

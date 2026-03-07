#ifndef ECS_ASSIMPMODELCOMPONENT_H
#define ECS_ASSIMPMODELCOMPONENT_H

#include <glm/glm.hpp>

class AssimpMesh;
class BoundingBox;

/// Pure POD component — replaces the old AssimpEntity OO class.
/// An entt::entity with this component represents an Assimp-loaded 3D model.
struct AssimpModelComponent {
    AssimpMesh*  mesh     = nullptr;
    glm::vec3    position = glm::vec3(0.0f);
    glm::vec3    rotation = glm::vec3(0.0f);
    float        scale    = 1.0f;
    BoundingBox* box      = nullptr;
};

#endif // ECS_ASSIMPMODELCOMPONENT_H

// src/ECS/Components/AssimpModelComponent.h
//
// Pure-data EnTT component that represents an Assimp-loaded scene object.
// Replaces the legacy AssimpEntity* side-vector with a plain POD struct.
//
// All transform data (position, rotation, scale) lives here directly so
// that MasterRenderer, AssimpEntityRenderer, FrustumCuller, ChunkManager,
// and StreamingSystem can read transform/geometry data without touching
// any legacy OO wrapper.

#ifndef ECS_ASSIMPMODELCOMPONENT_H
#define ECS_ASSIMPMODELCOMPONENT_H

#include <glm/glm.hpp>

class AssimpMesh;
class BoundingBox;

struct AssimpModelComponent {
    AssimpMesh*  mesh     = nullptr;            ///< Assimp geometry + material
    glm::vec3    position = glm::vec3(0.0f);    ///< world-space position
    glm::vec3    rotation = glm::vec3(0.0f);    ///< Euler rotation (degrees)
    float        scale    = 1.0f;               ///< uniform scale factor
    BoundingBox* box      = nullptr;            ///< optional AABB for culling/picking
};

#endif // ECS_ASSIMPMODELCOMPONENT_H

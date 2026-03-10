#ifndef ECS_ASSIMPMODELCOMPONENT_H
#define ECS_ASSIMPMODELCOMPONENT_H

#include <glm/glm.hpp>
#include <string>

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

    /// Path to the mesh file (e.g. "models/guard.glb").  Populated by
    /// EntityFactory from the prefab's "mesh" field.  The client's asset
    /// loader resolves this into an AssimpMesh* when the entity enters the
    /// render view.  Left empty for entities loaded via SceneLoader.
    std::string  meshPath;

    /// Phase 4 Step 4.3 — LOD mesh variations.
    /// LOD0 = mesh (high poly), LOD1 = mid poly, LOD2 = low poly.
    /// nullptr means no LOD variation is available for that level.
    AssimpMesh*  meshLOD1 = nullptr;
    AssimpMesh*  meshLOD2 = nullptr;
};

#endif // ECS_ASSIMPMODELCOMPONENT_H

// src/ECS/Components/AssimpModelComponent.h
//
// EnTT component that represents an Assimp-loaded scene object.
// This replaces the legacy AssimpComponent / AssimpEntity* side-vector.
//
// Primary data:
//   mesh   — the raw Assimp mesh (the authoritative geometry/material data)
//
// Compatibility shim (TODO: remove once MasterRenderer API is updated):
//   entity — the legacy AssimpEntity wrapper kept alive for the current
//             MasterRenderer::renderScene() API which takes AssimpEntity*.
//             Once MasterRenderer is refactored to accept AssimpModelComponent
//             data directly, this field will be removed.
//             (BoundingBox and transform data are still accessed via entity.)
//
// Transform (position/rotation/scale) may be stored in a companion
// TransformComponent on the same registry entity; for now the transform
// is still carried inside AssimpEntity.

#ifndef ECS_ASSIMPMODELCOMPONENT_H
#define ECS_ASSIMPMODELCOMPONENT_H

class AssimpMesh;
class AssimpEntity;

struct AssimpModelComponent {
    AssimpMesh*   mesh   = nullptr; ///< primary mesh data (key ECS field)
    AssimpEntity* entity = nullptr; ///< legacy wrapper kept for MasterRenderer compat
};

#endif // ECS_ASSIMPMODELCOMPONENT_H

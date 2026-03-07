// src/ECS/Components/AssimpComponent.h
//
// EnTT component that stores a raw pointer to a heap-allocated AssimpEntity.
// One registry entity is created per Assimp scene object.
//
// Ownership: AssimpEntity objects are heap-allocated by SceneLoader/SceneLoaderJson
// and are currently never explicitly freed (pre-existing behaviour carried forward
// from the old `scenes` side-vector which also leaked them).
// TODO: take ownership here (store unique_ptr<AssimpEntity>) once AssimpEntity is
//       made movable, eliminating this known leak.

#ifndef ECS_ASSIMPCOMPONENT_H
#define ECS_ASSIMPCOMPONENT_H

class AssimpEntity;

struct AssimpComponent {
    AssimpEntity* entity = nullptr; ///< non-owning; AssimpEntity lifetime is currently leaked
};

#endif // ECS_ASSIMPCOMPONENT_H

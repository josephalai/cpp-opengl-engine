// src/ECS/Components/EntityOwnerComponent.h
//
// EnTT component that stores a raw pointer back to the heap-allocated Entity
// façade that wraps this entt::entity handle.
//
// This is emplaced on every registry entity created via `new Entity(registry,
// ...)` so that Systems can retrieve the Entity* for MasterRenderer
// compatibility via registry.view<EntityOwnerComponent>().
//
// Ownership: Entity* objects are currently never explicitly deleted — this is
// a pre-existing leak carried forward from the old `entities` side-vector.
// TODO: take ownership here (store unique_ptr<Entity> with custom deleter that
//       guards against the double-destroy in Entity::~Entity) to eliminate
//       this known leak.
//
// Note: Entity::~Entity() calls registry.destroy(handle_), so no registry
// entity outlives its Entity* wrapper.

#ifndef ECS_ENTITYOWNERCOMPONENT_H
#define ECS_ENTITYOWNERCOMPONENT_H

class Entity;

struct EntityOwnerComponent {
    Entity* ptr = nullptr; ///< non-owning back-pointer; Entity* lifetime is currently leaked
};

#endif // ECS_ENTITYOWNERCOMPONENT_H

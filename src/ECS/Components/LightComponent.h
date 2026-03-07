// src/ECS/Components/LightComponent.h
//
// EnTT component that stores an owning raw pointer to a heap-allocated Light.
// One registry entity is created per scene light; Engine::shutdown() queries
// this component to delete the Light objects before the registry is destroyed.
//
// Ownership: Engine is responsible for deleting lc.light in shutdown().
// TODO: migrate to std::unique_ptr<Light> once Light supports move semantics.

#ifndef ECS_LIGHTCOMPONENT_H
#define ECS_LIGHTCOMPONENT_H

class Light;

struct LightComponent {
    Light* light = nullptr; ///< owning raw pointer; Engine::shutdown() must delete it
};

#endif // ECS_LIGHTCOMPONENT_H

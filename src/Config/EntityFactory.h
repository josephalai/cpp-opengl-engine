// src/Config/EntityFactory.h
//
// Data-driven entity spawner.  Given a prefab ID and a world position, the
// factory looks up the prefab JSON from PrefabManager, creates an entt::entity,
// and attaches ECS components based on the data it finds — without knowing
// what the entity *means* (guard, merchant, tree, etc.).
//
// This is the bridge between the data layer (JSON prefabs) and the ECS
// runtime.  The C++ engine never decides *what* to spawn; it only knows
// *how* to attach components declared in the data.
//
// Usage:
//   auto entity = EntityFactory::spawn(registry, "npc_guard", {100, 3, -80});

#ifndef ENGINE_ENTITY_FACTORY_H
#define ENGINE_ENTITY_FACTORY_H

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>

class PhysicsSystem;

class EntityFactory {
public:
    /// Spawn an entity from a prefab definition.
    ///
    /// @param registry    The ECS registry to create the entity in.
    /// @param prefabId    String ID matching a loaded prefab in PrefabManager.
    /// @param position    World-space spawn position (feet position).
    /// @param physics     Optional pointer to PhysicsSystem; if provided and
    ///                    the prefab declares a "physics" block, a character
    ///                    controller or rigid body will be registered.
    /// @param rotation    Optional initial Euler rotation in degrees.
    /// @param scale       Optional uniform scale factor (default 1.0).
    /// @return            The newly created entt::entity, or entt::null if
    ///                    the prefab ID was not found.
    static entt::entity spawn(entt::registry& registry,
                              const std::string& prefabId,
                              const glm::vec3& position,
                              PhysicsSystem* physics = nullptr,
                              const glm::vec3& rotation = glm::vec3(0.0f),
                              float scale = 1.0f);
};

#endif // ENGINE_ENTITY_FACTORY_H

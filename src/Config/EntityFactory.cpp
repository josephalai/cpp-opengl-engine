// src/Config/EntityFactory.cpp

#include "EntityFactory.h"
#include "PrefabManager.h"
#include "ConfigManager.h"

#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../ECS/Components/InputQueueComponent.h"
#include "../Physics/PhysicsSystem.h"

#include <nlohmann/json.hpp>
#include <iostream>

// -------------------------------------------------------------------------
// spawn — create an entity from a prefab definition
// -------------------------------------------------------------------------

entt::entity EntityFactory::spawn(entt::registry& registry,
                                  const std::string& prefabId,
                                  const glm::vec3& position,
                                  PhysicsSystem* physics) {
    const auto& prefab = PrefabManager::get().getPrefab(prefabId);
    if (prefab.is_null()) {
        std::cerr << "[EntityFactory] Unknown prefab: " << prefabId << "\n";
        return entt::null;
    }

    entt::entity entity = registry.create();

    // --- TransformComponent (always added) ---
    auto& tc = registry.emplace<TransformComponent>(entity);
    tc.position = position;

    // --- NetworkIdComponent (if "model_type" is present) ---
    if (prefab.contains("model_type")) {
        auto& nid = registry.emplace<NetworkIdComponent>(entity);
        nid.modelType = prefab["model_type"].get<std::string>();
        nid.isNPC     = true;  // prefab-spawned entities are NPCs by default
    }

    // --- InputStateComponent (use prefab overrides or ConfigManager defaults) ---
    {
        auto& isc = registry.emplace<InputStateComponent>(entity);
        const auto& cfg = ConfigManager::get().physics;
        isc.runSpeed  = cfg.defaultRunSpeed;
        isc.turnSpeed = cfg.defaultTurnSpeed;

        // Per-entity overrides from the prefab's "components" block.
        if (prefab.contains("components")) {
            const auto& comps = prefab["components"];
            if (comps.contains("InputStateComponent")) {
                const auto& isc_json = comps["InputStateComponent"];
                isc.runSpeed  = isc_json.value("run_speed",  isc.runSpeed);
                isc.turnSpeed = isc_json.value("turn_speed", isc.turnSpeed);
            }
        }
    }

    // --- InputQueueComponent (always added for entities that need ticking) ---
    registry.emplace<InputQueueComponent>(entity);

    // --- Physics character controller ---
    if (physics && prefab.contains("physics")) {
        const auto& phys = prefab["physics"];
        float radius = phys.value("radius",
                           ConfigManager::get().physics.defaultCapsuleRadius);
        float height = phys.value("height",
                           ConfigManager::get().physics.defaultCapsuleHeight);
        physics->addCharacterController(entity, radius, height);
    }

    return entity;
}

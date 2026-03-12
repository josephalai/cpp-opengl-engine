// src/Interaction/InteractionSystem.cpp
//
// Implementation of the server-side interaction state machine.
// C++ handles distance, timers, and Lua dispatch.
// Lua handles all game logic — this class has zero game design knowledge.

#include "InteractionSystem.h"

#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/ActionStateComponent.h"
#include "../ECS/Components/InteractableComponent.h"
#include "../ECS/Components/PathfindingComponent.h"

#include <glm/glm.hpp>
#include <iostream>

// -------------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------------

InteractionSystem::InteractionSystem(entt::registry& registry,
                                     LuaScriptEngine& luaEngine)
    : registry_(registry)
    , luaEngine_(luaEngine) {}

// -------------------------------------------------------------------------
// update — the blind state machine
// -------------------------------------------------------------------------

void InteractionSystem::update(float dt) {
    auto view = registry_.view<TransformComponent, ActionStateComponent>();

    for (auto player : view) {
        auto& tc     = view.get<TransformComponent>(player);
        auto& action = view.get<ActionStateComponent>(player);

        // 1. Validate target still exists.
        if (!registry_.valid(action.targetEntity)) {
            registry_.remove<ActionStateComponent>(player);
            continue;
        }

        // 2. Validate target still has an InteractableComponent.
        if (!registry_.all_of<InteractableComponent>(action.targetEntity)) {
            registry_.remove<ActionStateComponent>(player);
            continue;
        }

        auto& targetTc       = registry_.get<TransformComponent>(action.targetEntity);
        auto& targetInteract = registry_.get<InteractableComponent>(action.targetEntity);

        // 3. Check distance (flat XZ plane to avoid vertical terrain variation).
        float dist = glm::distance(
            glm::vec3(tc.position.x, 0.0f, tc.position.z),
            glm::vec3(targetTc.position.x, 0.0f, targetTc.position.z));

        if (dist > targetInteract.interactRange) {
            // Still walking — let PathfindingSystem do its job.
            continue;
        }

        // 4. We arrived — stop moving.
        if (!action.isArrived) {
            action.isArrived = true;
            if (registry_.all_of<PathfindingComponent>(player)) {
                registry_.remove<PathfindingComponent>(player);
            }
        }

        // 5. Tick the cooldown timer.
        action.actionTimer -= dt;
        if (action.actionTimer > 0.0f) {
            continue; // Waiting for the next action window.
        }

        // 6. CALL THE LUA SCRIPT — C++ has no knowledge of what happens here.
        // Pass the registry so executeInteraction can resolve NetworkIdComponent::id
        // and give Lua the real network IDs rather than raw entt handles.
        float cooldown = luaEngine_.executeInteraction(
            targetInteract.scriptPath, player, action.targetEntity, &registry_);

        // 7. State machine routing based on the returned cooldown.
        if (cooldown > 0.0f) {
            // Loop the action (e.g., keep chopping, keep attacking).
            action.actionTimer = cooldown;
        } else {
            // Action complete (e.g., dialogue opened, resource depleted).
            registry_.remove<ActionStateComponent>(player);
        }
    }
}

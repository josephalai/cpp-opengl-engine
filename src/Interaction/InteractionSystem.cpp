// src/Interaction/InteractionSystem.cpp
//
// Implementation of the blind server-side interaction state machine.

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
    , luaEngine_(luaEngine)
{}

// -------------------------------------------------------------------------
// update — process all active ActionStateComponents
// -------------------------------------------------------------------------

void InteractionSystem::update(float dt) {
    auto view = registry_.view<TransformComponent, ActionStateComponent>();

    for (auto player : view) {
        auto& tc     = view.get<TransformComponent>(player);
        auto& action = view.get<ActionStateComponent>(player);

        // 1. Validate the target entity still exists.
        if (!registry_.valid(action.targetEntity)) {
            registry_.remove<ActionStateComponent>(player);
            continue;
        }

        // 2. Validate the target still has an InteractableComponent.
        if (!registry_.all_of<InteractableComponent>(action.targetEntity)) {
            registry_.remove<ActionStateComponent>(player);
            continue;
        }

        auto& targetTc      = registry_.get<TransformComponent>(action.targetEntity);
        auto& targetInteract = registry_.get<InteractableComponent>(action.targetEntity);

        // 3. Check distance (flat XZ plane — ignore Y for slope traversal).
        //    Use squared distance to avoid a sqrt() on every tick.
        float dx = tc.position.x - targetTc.position.x;
        float dz = tc.position.z - targetTc.position.z;
        float distSq = dx * dx + dz * dz;
        float rangeSq = targetInteract.interactRange * targetInteract.interactRange;

        if (distSq > rangeSq) {
            // Still walking — let PathfindingSystem steer the player.
            continue;
        }

        // 4. First arrival: stop pathfinding.
        if (!action.isArrived) {
            action.isArrived = true;
            if (registry_.all_of<PathfindingComponent>(player)) {
                registry_.remove<PathfindingComponent>(player);
            }
        }

        // 5. Decrement the action timer.
        action.actionTimer -= dt;
        if (action.actionTimer > 0.0f) {
            continue; // Still waiting for the cooldown.
        }

        // 6. Timer fired — call the Lua script.
        float cooldown = luaEngine_.executeInteraction(
            targetInteract.scriptPath, player, action.targetEntity, &registry_);

        // 7. State machine routing based on the returned cooldown.
        if (cooldown > 0.0f) {
            // Continue looping (e.g., keep chopping the tree).
            action.actionTimer = cooldown;
        } else {
            // Interaction complete (e.g., dialogue opened, target destroyed).
            // Guard against the target already being destroyed by the script.
            if (registry_.valid(player)) {
                registry_.remove<ActionStateComponent>(player);
            }
        }
    }
}

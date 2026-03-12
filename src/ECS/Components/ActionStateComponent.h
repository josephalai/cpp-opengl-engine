// src/ECS/Components/ActionStateComponent.h
//
// Attached to a Player entity when they click an interactable object.
// Drives the "blind" server-side interaction state machine in InteractionSystem.
// The component is removed when the interaction ends (cooldown returns 0) or
// the target entity is destroyed.

#ifndef ENGINE_ACTION_STATE_COMPONENT_H
#define ENGINE_ACTION_STATE_COMPONENT_H

#include <entt/entt.hpp>

/// Tracks the active interaction state for a player.
struct ActionStateComponent {
    entt::entity targetEntity = entt::null; ///< The entity being interacted with.
    float        actionTimer  = 0.0f;       ///< Countdown timer; fires the Lua script when it hits 0.
    bool         isArrived    = false;      ///< True once the player is within interactRange.
};

#endif // ENGINE_ACTION_STATE_COMPONENT_H

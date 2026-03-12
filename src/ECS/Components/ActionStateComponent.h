// src/ECS/Components/ActionStateComponent.h
//
// Attached to a Player entity when they click an interactable object.
// Drives the server-side "blind" state machine inside InteractionSystem.
//
// State lifecycle:
//   1. Emplace on player when ActionRequestPacket arrives (isArrived=false).
//   2. InteractionSystem watches distance; PathfindingSystem steers toward target.
//   3. When within interactRange, isArrived=true and PathfindingComponent is removed.
//   4. actionTimer counts down each tick; when it hits 0, the Lua script is called.
//   5. If the script returns > 0, actionTimer is reset (repeat action).
//      If the script returns 0, the component is removed (action complete).

#ifndef ECS_ACTIONSTATECOMPONENT_H
#define ECS_ACTIONSTATECOMPONENT_H

#include <entt/entt.hpp>

/// Per-player interaction state.  Removed when the action completes or the
/// target entity is destroyed.
struct ActionStateComponent {
    entt::entity targetEntity = entt::null; ///< The entity being interacted with.
    float        actionTimer  = 0.0f;       ///< Countdown until next Lua script call.
    bool         isArrived    = false;      ///< True once the player is within interactRange.
};

#endif // ECS_ACTIONSTATECOMPONENT_H

// src/Interaction/InteractionSystem.h
//
// The "blind" server-side interaction state machine.
//
// InteractionSystem is completely agnostic about game design.  It knows only
// three things:
//   1. A player has a target (ActionStateComponent).
//   2. The target requires a minimum distance to interact (InteractableComponent).
//   3. When the player arrives and the cooldown fires, call a Lua script.
//
// All game logic — rewards, animations, resource depletion, damage — lives
// entirely in the Lua scripts.  C++ never hardcodes any of it.
//
// Usage (server tick loop):
//   InteractionSystem interactionSystem(registry, luaEngine);
//   // ... inside tick:
//   interactionSystem.update(dt);

#ifndef ENGINE_INTERACTION_SYSTEM_H
#define ENGINE_INTERACTION_SYSTEM_H

#include <entt/entt.hpp>
#include "../Scripting/LuaScriptEngine.h"

class InteractionSystem {
public:
    /// @param registry   The ECS registry shared with the server.
    /// @param luaEngine  The scripting engine used to call on_interact().
    InteractionSystem(entt::registry& registry, LuaScriptEngine& luaEngine);

    /// Process all active ActionStateComponents for one tick.
    /// Must be called AFTER PathfindingSystem::update() so that entity
    /// positions are up-to-date when distance is measured.
    /// @param dt  Delta time in seconds for this tick.
    void update(float dt);

private:
    entt::registry&  registry_;
    LuaScriptEngine& luaEngine_;
};

#endif // ENGINE_INTERACTION_SYSTEM_H

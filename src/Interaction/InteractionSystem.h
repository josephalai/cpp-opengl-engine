// src/Interaction/InteractionSystem.h
//
// The server-side "blind" state machine.  It is completely agnostic about
// game design: it only manages distance checking, timer countdown, and Lua
// script invocation.  All game logic (woodcutting, combat, dialogue) lives
// exclusively in the Lua scripts attached to InteractableComponent.
//
// Usage:
//   InteractionSystem system(registry, luaEngine);
//   // In server tick loop (after PathfindingSystem::update):
//   system.update(dt);

#ifndef ENGINE_INTERACTION_SYSTEM_H
#define ENGINE_INTERACTION_SYSTEM_H

#include <entt/entt.hpp>
#include "../Scripting/LuaScriptEngine.h"

class InteractionSystem {
public:
    InteractionSystem(entt::registry& registry, LuaScriptEngine& luaEngine);

    /// Update all active player interactions.  Must be called once per server
    /// tick, AFTER PathfindingSystem::update() so that distance checks see
    /// the most recent player position.
    ///
    /// @param dt  Elapsed time since last tick (seconds).
    void update(float dt);

private:
    entt::registry&  registry_;
    LuaScriptEngine& luaEngine_;
};

#endif // ENGINE_INTERACTION_SYSTEM_H

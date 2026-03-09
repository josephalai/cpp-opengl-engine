// src/ECS/Components/AIScriptComponent.h
//
// Lightweight ECS component that stores a reference to a Lua AI script.
// EntityFactory attaches this when a prefab declares an "ai_script" field.
// The LuaScriptEngine reads this component to know which script drives
// the entity's behaviour on the server.

#ifndef ECS_AISCRIPTCOMPONENT_H
#define ECS_AISCRIPTCOMPONENT_H

#include <string>

struct AIScriptComponent {
    /// Path to the Lua script file (relative to Resources), e.g.
    /// "scripts/ai/guard_behavior.lua".
    std::string scriptPath;

    /// Logical script name (e.g. "GuardAI", "WanderAI") used as a lookup
    /// key when the LuaScriptEngine dispatches tick updates.
    std::string scriptName;
};

#endif // ECS_AISCRIPTCOMPONENT_H

// src/Scripting/LuaScriptEngine.h
//
// Lua scripting bridge for data-driven NPC AI.
// Wraps Sol2/Lua state and exposes a minimal C++ API so that AI behaviour
// can be defined in .lua files instead of hardcoded C++ switch statements.
//
// Usage:
//   LuaScriptEngine lua;
//   lua.init(resourceRoot);
//   lua.loadScript("scripts/ai/wander.lua");
//   auto result = lua.tickAI("WanderAI", entityId, dt, aiState);

#ifndef ENGINE_LUA_SCRIPT_ENGINE_H
#define ENGINE_LUA_SCRIPT_ENGINE_H

#ifdef HAS_LUA

#include <string>
#include <unordered_map>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "../Network/NetworkPackets.h"

/// Per-NPC AI state exposed to Lua as a mutable table.
struct LuaAIState {
    float timer     = 0.0f;
    int   phase     = 0;
    float cameraYaw = 0.0f;
};

/// Result of a Lua AI tick — mirrors the synthetic PlayerInputPacket fields
/// that the server needs.
struct LuaAIResult {
    bool  moveForward  = false;
    bool  moveBackward = false;
    bool  moveLeft     = false;
    bool  moveRight    = false;
    bool  jump         = false;
    float cameraYaw    = 0.0f;
    float deltaTime    = 0.0f;
};

class LuaScriptEngine {
public:
    LuaScriptEngine();
    ~LuaScriptEngine();

    /// Initialise the Lua state, register C++ types, and set the script
    /// search path.  @p resourceRoot is the engine's RESOURCE_ROOT.
    void init(const std::string& resourceRoot);

    /// Load a Lua script file (relative to resourceRoot/src/Resources/).
    /// The script should define a global function named after the AI type,
    /// e.g.  function WanderAI(state, dt)  ... return result end
    bool loadScript(const std::string& relativePath);

    /// Tick an AI script for one entity.
    /// @param scriptName  Global function name in Lua (e.g. "WanderAI").
    /// @param entityId    Network entity ID (for logging).
    /// @param dt          Delta time for this tick.
    /// @param state       [in/out] Persistent AI state for this entity.
    /// @return            The input packet to feed into the entity's queue.
    Network::PlayerInputPacket tickAI(const std::string& scriptName,
                                      uint32_t entityId,
                                      float dt,
                                      LuaAIState& state);

    /// Check whether a named AI function is available.
    bool hasScript(const std::string& scriptName) const;

    /// Shut down the Lua state.
    void shutdown();

private:
    sol::state lua_;
    std::string resourceRoot_;
    bool initialised_ = false;
};

#else // !HAS_LUA — stub so code compiles without Lua installed

#include <string>
#include "../Network/NetworkPackets.h"

struct LuaAIState {
    float timer     = 0.0f;
    int   phase     = 0;
    float cameraYaw = 0.0f;
};

class LuaScriptEngine {
public:
    void init(const std::string&) {}
    bool loadScript(const std::string&) { return false; }
    Network::PlayerInputPacket tickAI(const std::string&, uint32_t, float,
                                      LuaAIState&) { return {}; }
    bool hasScript(const std::string&) const { return false; }
    void shutdown() {}
};

#endif // HAS_LUA
#endif // ENGINE_LUA_SCRIPT_ENGINE_H

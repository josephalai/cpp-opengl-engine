// src/Scripting/LuaScriptEngine.h
//
// Lua scripting bridge for data-driven NPC AI and entity interactions.
// Wraps Sol2/Lua state and exposes a minimal C++ API so that AI behaviour
// and gameplay interactions can be defined in .lua files instead of hardcoded
// C++ switch statements.
//
// Usage (NPC AI):
//   LuaScriptEngine lua;
//   lua.init(resourceRoot);
//   lua.loadScript("scripts/ai/wander.lua");
//   auto result = lua.tickAI("WanderAI", entityId, dt, aiState);
//
// Usage (Interactions):
//   float cooldown = lua.executeInteraction("scripts/skills/woodcutting.lua",
//                                           playerEntity, targetEntity);

#ifndef ENGINE_LUA_SCRIPT_ENGINE_H
#define ENGINE_LUA_SCRIPT_ENGINE_H

#ifdef HAS_LUA

#include <string>
#include <unordered_map>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <entt/entt.hpp>
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

    /// Execute an interaction script's on_interact() function.
    ///
    /// Each script runs in an isolated Lua environment so multiple scripts
    /// can all define on_interact() without clobbering each other.
    ///
    /// @param scriptPath  Relative path to the .lua file (e.g. "scripts/skills/woodcutting.lua").
    /// @param player      The player entity performing the action.
    /// @param target      The target entity being interacted with.
    /// @return            Cooldown time in seconds; 0.0 means the action is complete.
    float executeInteraction(const std::string& scriptPath,
                             entt::entity player,
                             entt::entity target);

    /// Check whether a named AI function is available.
    bool hasScript(const std::string& scriptName) const;

    /// Shut down the Lua state.
    void shutdown();

private:
    /// Build the `engine` table passed to every on_interact() call.
    /// Contains sub-tables: Network, Stats, Inventory, Health, Entities,
    /// Math, Transform, CombatMath, Equipment, Loot.
    sol::table buildEngineTable(entt::entity player, entt::entity target);

    sol::state lua_;
    std::string resourceRoot_;
    bool initialised_ = false;

    /// Cache of loaded interaction script environments, keyed by script path.
    std::unordered_map<std::string, sol::environment> interactionEnvs_;
};

#else // !HAS_LUA — stub so code compiles without Lua installed

#include <string>
#include <entt/entt.hpp>
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
    float executeInteraction(const std::string&, entt::entity, entt::entity) { return 0.0f; }
    bool hasScript(const std::string&) const { return false; }
    void shutdown() {}
};

#endif // HAS_LUA
#endif // ENGINE_LUA_SCRIPT_ENGINE_H

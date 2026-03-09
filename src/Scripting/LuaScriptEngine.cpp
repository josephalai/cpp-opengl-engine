// src/Scripting/LuaScriptEngine.cpp
//
// Implementation of the Lua scripting bridge for data-driven NPC AI.

#include "LuaScriptEngine.h"

#ifdef HAS_LUA

#include "../Config/ConfigManager.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// -------------------------------------------------------------------------
// Constructor / Destructor
// -------------------------------------------------------------------------

LuaScriptEngine::LuaScriptEngine() = default;

LuaScriptEngine::~LuaScriptEngine() {
    shutdown();
}

// -------------------------------------------------------------------------
// init — set up the Lua state and register C++ types
// -------------------------------------------------------------------------

void LuaScriptEngine::init(const std::string& resourceRoot) {
    resourceRoot_ = resourceRoot;

    // Open standard Lua libraries (math, string, table, etc.)
    lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                        sol::lib::table);

    // --- Register LuaAIState as a Lua usertype ---
    lua_.new_usertype<LuaAIState>("AIState",
        "timer",     &LuaAIState::timer,
        "phase",     &LuaAIState::phase,
        "cameraYaw", &LuaAIState::cameraYaw
    );

    // --- Register LuaAIResult as a Lua usertype ---
    lua_.new_usertype<LuaAIResult>("AIResult",
        sol::constructors<LuaAIResult()>(),
        "moveForward",  &LuaAIResult::moveForward,
        "moveBackward", &LuaAIResult::moveBackward,
        "moveLeft",     &LuaAIResult::moveLeft,
        "moveRight",    &LuaAIResult::moveRight,
        "jump",         &LuaAIResult::jump,
        "cameraYaw",    &LuaAIResult::cameraYaw,
        "deltaTime",    &LuaAIResult::deltaTime
    );

    // --- Expose ConfigManager NPC turn speed to Lua ---
    // Scripts can read  config.npcTurnSpeed  to stay data-driven.
    sol::table config = lua_.create_named_table("config");
    config["npcTurnSpeed"] = ConfigManager::get().physics.npcTurnSpeed;
    config["runSpeed"]     = ConfigManager::get().physics.defaultRunSpeed;
    config["turnSpeed"]    = ConfigManager::get().physics.defaultTurnSpeed;
    config["gravity"]      = ConfigManager::get().physics.gravity.y;
    config["jumpPower"]    = ConfigManager::get().physics.jumpPower;

    initialised_ = true;
    std::cout << "[LuaScriptEngine] Initialised Lua state.\n";
}

// -------------------------------------------------------------------------
// loadScript — load and execute a .lua file
// -------------------------------------------------------------------------

bool LuaScriptEngine::loadScript(const std::string& relativePath) {
    if (!initialised_) {
        std::cerr << "[LuaScriptEngine] Cannot load script — not initialised.\n";
        return false;
    }

    std::string fullPath = resourceRoot_ + "/src/Resources/" + relativePath;

    if (!fs::exists(fullPath)) {
        std::cerr << "[LuaScriptEngine] Script not found: " << fullPath << "\n";
        return false;
    }

    auto result = lua_.safe_script_file(fullPath, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "[LuaScriptEngine] Error loading " << relativePath
                  << ": " << err.what() << "\n";
        return false;
    }

    std::cout << "[LuaScriptEngine] Loaded: " << relativePath << "\n";
    return true;
}

// -------------------------------------------------------------------------
// tickAI — call a named Lua AI function
// -------------------------------------------------------------------------

Network::PlayerInputPacket LuaScriptEngine::tickAI(
    const std::string& scriptName,
    uint32_t /*entityId*/,
    float dt,
    LuaAIState& state)
{
    Network::PlayerInputPacket pkt{};
    pkt.deltaTime = dt;

    if (!initialised_) return pkt;

    sol::protected_function fn = lua_[scriptName];
    if (!fn.valid()) {
        // Script function not found — return empty packet.
        return pkt;
    }

    auto callResult = fn(state, dt);
    if (!callResult.valid()) {
        sol::error err = callResult;
        std::cerr << "[LuaScriptEngine] Error in " << scriptName
                  << ": " << err.what() << "\n";
        return pkt;
    }

    // The Lua function returns an AIResult table.
    LuaAIResult res = callResult.get<LuaAIResult>();

    pkt.moveForward  = res.moveForward;
    pkt.moveBackward = res.moveBackward;
    pkt.moveLeft     = res.moveLeft;
    pkt.moveRight    = res.moveRight;
    pkt.jump         = res.jump;
    pkt.cameraYaw    = res.cameraYaw;

    return pkt;
}

// -------------------------------------------------------------------------
// hasScript — check whether a named AI function exists
// -------------------------------------------------------------------------

bool LuaScriptEngine::hasScript(const std::string& scriptName) const {
    if (!initialised_) return false;
    sol::object fn = lua_[scriptName];
    return fn.valid() && fn.is<sol::function>();
}

// -------------------------------------------------------------------------
// shutdown
// -------------------------------------------------------------------------

void LuaScriptEngine::shutdown() {
    initialised_ = false;
}

#endif // HAS_LUA

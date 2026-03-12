// src/Scripting/LuaScriptEngine.cpp
//
// Implementation of the Lua scripting bridge for data-driven NPC AI
// and interaction scripts.

#include "LuaScriptEngine.h"

#ifdef HAS_LUA

#include "../Config/ConfigManager.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include <iostream>
#include <filesystem>
#include <random>

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
    uint32_t entityId,
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
                  << " (entity " << entityId << "): " << err.what() << "\n";
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
// executeInteraction — call a Lua interaction script's on_interact()
// -------------------------------------------------------------------------

float LuaScriptEngine::executeInteraction(const std::string& scriptPath,
                                          entt::entity player,
                                          entt::entity target,
                                          entt::registry* registry)
{
    if (!initialised_) return 0.0f;

    // Resolve network IDs for passing to Lua; fall back to raw integral value.
    uint32_t playerId = static_cast<uint32_t>(entt::to_integral(player));
    uint32_t targetId = static_cast<uint32_t>(entt::to_integral(target));
    if (registry) {
        if (auto* nid = registry->try_get<NetworkIdComponent>(player))
            playerId = nid->id;
        if (auto* nid = registry->try_get<NetworkIdComponent>(target))
            targetId = nid->id;
    }

    // -----------------------------------------------------------------
    // Ensure a per-script isolated environment exists and is loaded.
    // -----------------------------------------------------------------
    auto envIt = interactionEnvs_.find(scriptPath);
    if (envIt == interactionEnvs_.end()) {
        // Create a fresh environment that inherits the global Lua state.
        sol::environment env(lua_, sol::create, lua_.globals());

        std::string fullPath = resourceRoot_ + "/src/Resources/" + scriptPath;
        if (!fs::exists(fullPath)) {
            std::cerr << "[LuaScriptEngine] Interaction script not found: "
                      << fullPath << "\n";
            return 0.0f;
        }

        auto loadResult = lua_.safe_script_file(fullPath,
                                                env,
                                                sol::script_pass_on_error);
        if (!loadResult.valid()) {
            sol::error err = loadResult;
            std::cerr << "[LuaScriptEngine] Error loading interaction script "
                      << scriptPath << ": " << err.what() << "\n";
            return 0.0f;
        }

        envIt = interactionEnvs_.emplace(scriptPath, std::move(env)).first;
        std::cout << "[LuaScriptEngine] Loaded interaction script: "
                  << scriptPath << "\n";
    }

    sol::environment& env = envIt->second;

    // -----------------------------------------------------------------
    // Build the 'engine' API table passed to on_interact().
    // Each sub-table provides functional stubs that log to stdout and
    // return sensible defaults.  These stubs define the exact API
    // contract that real subsystem implementations will fill later.
    // -----------------------------------------------------------------
    sol::table engineApi = lua_.create_table();

    // --- engine.Network ---
    sol::table netApi = lua_.create_table();
    netApi["broadcastAnimation"] = [](uint32_t pid, const std::string& anim) {
        std::cout << "[Lua] broadcastAnimation(" << pid << ", \"" << anim << "\")\n";
    };
    netApi["sendMessage"] = [](uint32_t pid, const std::string& msg) {
        std::cout << "[Lua] sendMessage(" << pid << ", \"" << msg << "\")\n";
    };
    netApi["sendOpenUI"] = [](uint32_t pid, const std::string& ui) {
        std::cout << "[Lua] sendOpenUI(" << pid << ", \"" << ui << "\")\n";
    };
    netApi["broadcastDamageSplat"] = [](uint32_t tid, int dmg) {
        std::cout << "[Lua] broadcastDamageSplat(" << tid << ", " << dmg << ")\n";
    };
    engineApi["Network"] = netApi;

    // --- engine.Stats ---
    sol::table statsApi = lua_.create_table();
    statsApi["getLevel"] = [](uint32_t /*eid*/, const std::string& skill) -> int {
        std::cout << "[Lua] Stats.getLevel(skill=\"" << skill << "\") -> 1\n";
        return 1;
    };
    statsApi["getAll"] = [&](uint32_t eid) -> sol::table {
        std::cout << "[Lua] Stats.getAll(" << eid << ") -> {}\n";
        sol::table t = lua_.create_table();
        t["attack"]  = 1;
        t["defence"] = 1;
        t["strength"] = 1;
        return t;
    };
    engineApi["Stats"] = statsApi;

    // --- engine.Inventory ---
    sol::table invApi = lua_.create_table();
    invApi["addItem"] = [](uint32_t pid, const std::string& item, int count) {
        std::cout << "[Lua] Inventory.addItem(" << pid << ", \""
                  << item << "\", " << count << ")\n";
    };
    engineApi["Inventory"] = invApi;

    // --- engine.Health ---
    sol::table healthApi = lua_.create_table();
    healthApi["dealDamage"] = [](uint32_t tid, int amount) {
        std::cout << "[Lua] Health.dealDamage(" << tid << ", " << amount << ")\n";
    };
    healthApi["isDead"] = [](uint32_t /*tid*/) -> bool {
        return false;
    };
    engineApi["Health"] = healthApi;

    // --- engine.Entities ---
    // Capture registry pointer by value so the lambda can safely destroy.
    entt::registry* reg = registry;
    entt::entity    tgt = target;
    sol::table entApi   = lua_.create_table();
    entApi["destroy"] = [reg, tgt](uint32_t /*tid*/) {
        std::cout << "[Lua] Entities.destroy(target)\n";
        if (reg && reg->valid(tgt)) {
            reg->destroy(tgt);
        }
    };
    engineApi["Entities"] = entApi;

    // --- engine.Math ---
    sol::table mathApi = lua_.create_table();
    mathApi["rollChance"] = [](double probability) -> bool {
        // Deterministic RNG seeded per-call for testability.
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        bool result = dist(rng) < probability;
        std::cout << "[Lua] Math.rollChance(" << probability
                  << ") -> " << (result ? "true" : "false") << "\n";
        return result;
    };
    engineApi["Math"] = mathApi;

    // --- engine.Transform ---
    sol::table transformApi = lua_.create_table();
    transformApi["lookAt"] = [](uint32_t eid, uint32_t tid) {
        std::cout << "[Lua] Transform.lookAt(" << eid << ", " << tid << ")\n";
    };
    engineApi["Transform"] = transformApi;

    // --- engine.CombatMath ---
    sol::table combatApi = lua_.create_table();
    combatApi["calculateMeleeHit"] = [&](sol::table /*attacker*/,
                                         sol::table /*defender*/) -> int {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, 4);
        int dmg = dist(rng);
        std::cout << "[Lua] CombatMath.calculateMeleeHit() -> " << dmg << "\n";
        return dmg;
    };
    engineApi["CombatMath"] = combatApi;

    // --- engine.Equipment ---
    sol::table equipApi = lua_.create_table();
    equipApi["getWeaponSpeed"] = [](uint32_t /*pid*/) -> float {
        std::cout << "[Lua] Equipment.getWeaponSpeed() -> 2.4\n";
        return 2.4f;
    };
    engineApi["Equipment"] = equipApi;

    // --- engine.Loot ---
    sol::table lootApi = lua_.create_table();
    lootApi["generateDrop"] = [](uint32_t tid, const std::string& table) {
        std::cout << "[Lua] Loot.generateDrop(" << tid
                  << ", \"" << table << "\")\n";
    };
    engineApi["Loot"] = lootApi;

    // -----------------------------------------------------------------
    // Call on_interact(player_id, target_id, engine)
    // -----------------------------------------------------------------
    sol::protected_function fn = env["on_interact"];
    if (!fn.valid()) {
        std::cerr << "[LuaScriptEngine] on_interact not defined in "
                  << scriptPath << "\n";
        return 0.0f;
    }

    auto callResult = fn(playerId, targetId, engineApi);
    if (!callResult.valid()) {
        sol::error err = callResult;
        std::cerr << "[LuaScriptEngine] Error in on_interact (" << scriptPath
                  << "): " << err.what() << "\n";
        return 0.0f;
    }

    float cooldown = 0.0f;
    if (callResult.valid()) {
        sol::optional<float> opt = callResult;
        if (opt) cooldown = *opt;
    }
    return cooldown;
}

// -------------------------------------------------------------------------
// shutdown
// -------------------------------------------------------------------------

void LuaScriptEngine::shutdown() {
    interactionEnvs_.clear();
    initialised_ = false;
}

#endif // HAS_LUA

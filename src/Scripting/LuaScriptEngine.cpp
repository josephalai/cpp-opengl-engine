// src/Scripting/LuaScriptEngine.cpp
//
// Implementation of the Lua scripting bridge for data-driven NPC AI
// and entity interactions.

#include "LuaScriptEngine.h"

#ifdef HAS_LUA

#include "../Config/ConfigManager.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <filesystem>
#include <random>
#include <ctime>

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

    // Open standard Lua libraries (math, string, table, os, etc.)
    lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                        sol::lib::table, sol::lib::os);

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
// shutdown
// -------------------------------------------------------------------------

void LuaScriptEngine::shutdown() {
    interactionEnvs_.clear();
    initialised_ = false;
}

// -------------------------------------------------------------------------
// buildEngineTable — construct the `engine` API table passed to on_interact()
//
// Each sub-table is a collection of functional stub functions that log their
// actions to stdout and return sensible defaults.  They define the exact API
// contract that will be filled with real implementations in a future step.
// -------------------------------------------------------------------------

sol::table LuaScriptEngine::buildEngineTable(entt::entity player,
                                              entt::entity target,
                                              entt::registry* registry) {
    // player and target are reserved for future use: in a full implementation
    // these will be used to look up NetworkIdComponent so that the engine API
    // functions can default to the calling entities' network IDs when no
    // explicit ID is passed from Lua.
    (void)player;
    (void)target;

    sol::table engine = lua_.create_table();

    // --- engine.Network ---
    sol::table net = lua_.create_table();
    net["broadcastAnimation"] = [](uint32_t pid, const std::string& anim) {
        std::cout << "[Lua] broadcastAnimation(" << pid << ", \"" << anim << "\")\n";
    };
    net["sendMessage"] = [this](uint32_t pid, const std::string& msg) {
        std::cout << "[Lua] sendMessage(" << pid << ", \"" << msg << "\")\n";
        // Forward to the C++ server closure so an actual ENet packet is sent.
        if (onSendMessage_) {
            onSendMessage_(pid, msg);
        }
    };
    net["sendOpenUI"] = [](uint32_t pid, const std::string& ui) {
        std::cout << "[Lua] sendOpenUI(" << pid << ", \"" << ui << "\")\n";
    };
    net["broadcastDamageSplat"] = [](uint32_t tid, int dmg) {
        std::cout << "[Lua] broadcastDamageSplat(" << tid << ", " << dmg << ")\n";
    };
    engine["Network"] = net;

    // --- engine.Stats ---
    sol::table stats = lua_.create_table();
    stats["getLevel"] = [](uint32_t /*eid*/, const std::string& skill) -> int {
        std::cout << "[Lua] Stats.getLevel(_, \"" << skill << "\") -> 1\n";
        return 1;
    };
    stats["getAll"] = [&](uint32_t /*eid*/) -> sol::table {
        sol::table t = lua_.create_table();
        t["Attack"]     = 1;
        t["Defence"]    = 1;
        t["Strength"]   = 1;
        t["Woodcutting"] = 1;
        t["Hitpoints"]  = 10;
        return t;
    };
    engine["Stats"] = stats;

    // --- engine.Inventory ---
    sol::table inv = lua_.create_table();
    inv["addItem"] = [](uint32_t pid, const std::string& item, int count) {
        std::cout << "[Lua] Inventory.addItem(" << pid << ", \"" << item
                  << "\", " << count << ")\n";
    };
    inv["hasItem"] = [](uint32_t pid, const std::string& item) -> bool {
        std::cout << "[Lua] Inventory.hasItem(" << pid << ", \"" << item << "\") -> false\n";
        return false; // Stub: always returns false until inventory system is implemented
    };
    engine["Inventory"] = inv;

    // --- engine.Health ---
    sol::table health = lua_.create_table();
    health["dealDamage"] = [](uint32_t tid, int amount) {
        std::cout << "[Lua] Health.dealDamage(" << tid << ", " << amount << ")\n";
    };
    health["isDead"] = [](uint32_t /*tid*/) -> bool {
        return false;
    };
    engine["Health"] = health;

    // --- engine.Entities ---
    sol::table ents = lua_.create_table();
    ents["destroy"] = [](uint32_t /*tid*/) {
        // In a full implementation this would look up the entity by network ID
        // and destroy it. For now, log the intent.
        std::cout << "[Lua] Entities.destroy() — entity marked for removal\n";
    };
    engine["Entities"] = ents;

    // --- engine.Math ---
    sol::table math = lua_.create_table();
    math["rollChance"] = [](float probability) -> bool {
        // LCG seeded once per thread from a non-deterministic source so
        // that gameplay varies across server restarts.
        static thread_local unsigned int seed = [] {
            std::random_device rd;
            return rd();
        }();
        seed = seed * 1664525u + 1013904223u; // LCG advance
        float r = static_cast<float>(seed & 0x7FFF) / static_cast<float>(0x8000);
        return r < probability;
    };
    engine["Math"] = math;

    // --- engine.Transform ---
    sol::table transform = lua_.create_table();
    transform["lookAt"] = [this, registry](uint32_t lookerId, uint32_t targetId) {
        if (!registry) {
            std::cout << "[Lua] Transform.lookAt(" << lookerId << ", "
                      << targetId << ") — no registry\n";
            return;
        }

        entt::entity lookerEnt = entt::null;
        entt::entity targetEnt = entt::null;

        // Find the two entities by their NetworkIdComponent id.
        auto view = registry->view<NetworkIdComponent>();
        for (auto e : view) {
            uint32_t id = view.get<NetworkIdComponent>(e).id;
            if (id == lookerId) lookerEnt = e;
            if (id == targetId) targetEnt = e;
            if (lookerEnt != entt::null && targetEnt != entt::null) break;
        }

        if (lookerEnt == entt::null || targetEnt == entt::null) {
            std::cout << "[Lua] Transform.lookAt: entity not found ("
                      << lookerId << " → " << targetId << ")\n";
            return;
        }

        auto* lookerTc = registry->try_get<TransformComponent>(lookerEnt);
        auto* targetTc = registry->try_get<TransformComponent>(targetEnt);
        if (!lookerTc || !targetTc) return;

        glm::vec3 dir = targetTc->position - lookerTc->position;
        dir.y = 0.0f;  // ignore height difference — rotate on Y axis only
        if (glm::length(dir) > 0.001f) {
            dir = glm::normalize(dir);
            // rotation.y is in degrees (SharedMovement uses glm::radians to convert)
            float newYaw = glm::degrees(std::atan2(dir.x, dir.z));
            lookerTc->rotation.y = newYaw;

            // Sync the new yaw into the NPC AI state so the AI does not
            // overwrite this rotation on the very next tick.
            if (onSetNpcYaw_) {
                onSetNpcYaw_(lookerId, newYaw);
            }
        }
    };
    engine["Transform"] = transform;

    // --- engine.CombatMath ---
    sol::table combat = lua_.create_table();
    combat["calculateMeleeHit"] = [](sol::table /*attackerStats*/,
                                     sol::table /*defenderStats*/) -> int {
        // Placeholder: returns 1-5 damage until real combat math is added.
        static thread_local unsigned int seed = [] {
            std::random_device rd;
            return rd();
        }();
        seed = seed * 1664525u + 1013904223u;
        int dmg = static_cast<int>(seed % 5) + 1; // 1-5 damage
        std::cout << "[Lua] CombatMath.calculateMeleeHit() -> " << dmg << "\n";
        return dmg;
    };
    engine["CombatMath"] = combat;

    // --- engine.Equipment ---
    sol::table equip = lua_.create_table();
    equip["getWeaponSpeed"] = [](uint32_t /*pid*/) -> float {
        std::cout << "[Lua] Equipment.getWeaponSpeed() -> 2.4\n";
        return 2.4f; // Default OSRS attack speed (4-tick cycle)
    };
    engine["Equipment"] = equip;

    // --- engine.Loot ---
    sol::table loot = lua_.create_table();
    loot["generateDrop"] = [](uint32_t tid, const std::string& table) {
        std::cout << "[Lua] Loot.generateDrop(" << tid << ", \"" << table << "\")\n";
    };
    engine["Loot"] = loot;

    // --- engine.AI ---
    // Allows interaction scripts to control NPC behaviour:
    //   engine.AI.pause(npc_id, seconds)  — freezes NPC movement for N seconds.
    sol::table ai = lua_.create_table();
    ai["pause"] = [this](uint32_t npcId, float duration) {
        std::cout << "[Lua] AI.pause(" << npcId << ", " << duration << ")\n";
        if (onPauseNpc_) {
            onPauseNpc_(npcId, duration);
        }
    };
    engine["AI"] = ai;

    return engine;
}

// -------------------------------------------------------------------------
// executeInteraction — call on_interact() in an isolated script environment
// -------------------------------------------------------------------------

float LuaScriptEngine::executeInteraction(const std::string& scriptPath,
                                          entt::entity player,
                                          entt::entity target,
                                          entt::registry* registry) {
    if (!initialised_) return 0.0f;

    // Resolve the full filesystem path.
    std::string fullPath = resourceRoot_ + "/src/Resources/" + scriptPath;

    // 1. Load the script into an isolated environment the first time we see it.
    auto envIt = interactionEnvs_.find(scriptPath);
    if (envIt == interactionEnvs_.end()) {
        if (!fs::exists(fullPath)) {
            std::cerr << "[LuaScriptEngine] Interaction script not found: "
                      << fullPath << "\n";
            return 0.0f;
        }

        // Create a fresh environment inheriting global libs (math, string, etc.)
        sol::environment env(lua_, sol::create, lua_.globals());

        auto loadResult = lua_.safe_script_file(
            fullPath, env, sol::script_pass_on_error);
        if (!loadResult.valid()) {
            sol::error err = loadResult;
            std::cerr << "[LuaScriptEngine] Error loading interaction script "
                      << scriptPath << ": " << err.what() << "\n";
            return 0.0f;
        }

        interactionEnvs_.emplace(scriptPath, std::move(env));
        std::cout << "[LuaScriptEngine] Loaded interaction script: "
                  << scriptPath << "\n";
        envIt = interactionEnvs_.find(scriptPath);
    }

    sol::environment& env = envIt->second;

    // 2. Look up the on_interact function in this script's environment.
    sol::protected_function fn = env["on_interact"];
    if (!fn.valid()) {
        std::cerr << "[LuaScriptEngine] on_interact not defined in: "
                  << scriptPath << "\n";
        return 0.0f;
    }

    // 3. Build the engine API table and resolve entity IDs for Lua.
    sol::table engineTable = buildEngineTable(player, target, registry);

    // Prefer NetworkIdComponent::id so that engine.Network.sendMessage()
    // can match peerToNetworkId correctly on the server side.  Fall back to
    // the raw entt integral only when no registry / component is available.
    uint32_t playerId = static_cast<uint32_t>(entt::to_integral(player));
    uint32_t targetId = static_cast<uint32_t>(entt::to_integral(target));
    if (registry) {
        if (auto* nid = registry->try_get<NetworkIdComponent>(player)) {
            playerId = nid->id;
        }
        if (auto* nid = registry->try_get<NetworkIdComponent>(target)) {
            targetId = nid->id;
        }
    }

    // 4. Call on_interact(player_id, target_id, engine).
    auto callResult = fn(playerId, targetId, engineTable);
    if (!callResult.valid()) {
        sol::error err = callResult;
        std::cerr << "[LuaScriptEngine] Error in on_interact ("
                  << scriptPath << "): " << err.what() << "\n";
        return 0.0f;
    }

    // 5. Return the cooldown float.
    return callResult.get<float>(0);
}

#endif // HAS_LUA

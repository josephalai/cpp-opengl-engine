// src/Server/ServerNPCManager.cpp

#include "ServerNPCManager.h"
#include "../Config/ConfigManager.h"
#include "../Config/PrefabManager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// -------------------------------------------------------------------------
// loadFromJson — parse npcs.json (Phase 1, Step 3)
//
// Expected JSON structure:
//   {
//     "npcs": [
//       { "npc_id": 1, "prefab": "npc_wanderer", "model_type": "npc_wanderer",
//         "position": { "x": 110.0, "y": 3.0, "z": -70.0 }, "script": "WanderAI" },
//       ...
//     ]
//   }
// -------------------------------------------------------------------------

std::vector<NPCDefinition> ServerNPCManager::loadFromJson(const std::string& filePath) {
    std::vector<NPCDefinition> defs;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[ServerNPCManager] Could not open JSON file: " << filePath << "\n";
        return defs;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[ServerNPCManager] JSON parse error in " << filePath
                  << ": " << e.what() << "\n";
        return defs;
    }

    if (!root.contains("npcs") || !root["npcs"].is_array()) {
        std::cerr << "[ServerNPCManager] JSON missing top-level \"npcs\" array: "
                  << filePath << "\n";
        return defs;
    }

    for (const auto& entry : root["npcs"]) {
        NPCDefinition def;
        if (entry.contains("npc_id"))    def.npcId      = entry["npc_id"].get<uint32_t>();
        if (entry.contains("model_type"))def.modelType  = entry["model_type"].get<std::string>();
        if (entry.contains("script"))    def.scriptType = entry["script"].get<std::string>();
        if (entry.contains("prefab"))    def.prefab     = entry["prefab"].get<std::string>();
        if (entry.contains("position")) {
            const auto& pos = entry["position"];
            if (pos.contains("x")) def.startPos.x = pos["x"].get<float>();
            if (pos.contains("y")) def.startPos.y = pos["y"].get<float>();
            if (pos.contains("z")) def.startPos.z = pos["z"].get<float>();
        }
        defs.push_back(def);
    }

    std::cout << "[ServerNPCManager] Loaded " << defs.size()
              << " NPC(s) from JSON: " << filePath << "\n";
    return defs;
}

// -------------------------------------------------------------------------
// loadConfig — parse server_npcs.cfg (legacy pipe-separated format)
//
// Expected format (one NPC per line, '#' comments, blank lines ignored):
//   NPC_ID | MODEL_TYPE | START_X | START_Y | START_Z | SCRIPT_TYPE
// -------------------------------------------------------------------------

std::vector<NPCDefinition> ServerNPCManager::loadConfig(const std::string& filePath) {
    std::vector<NPCDefinition> defs;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[ServerNPCManager] Could not open " << filePath << "\n";
        return defs;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip leading whitespace.
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip comments and blank lines.
        if (line.empty() || line[0] == '#') continue;

        // Replace '|' with space for easier parsing.
        std::replace(line.begin(), line.end(), '|', ' ');

        std::istringstream iss(line);
        NPCDefinition def;
        if (iss >> def.npcId >> def.modelType
                >> def.startPos.x >> def.startPos.y >> def.startPos.z
                >> def.scriptType) {
            defs.push_back(def);
        } else {
            std::cerr << "[ServerNPCManager] Skipping malformed line: "
                      << line << "\n";
        }
    }

    std::cout << "[ServerNPCManager] Loaded " << defs.size()
              << " NPC(s) from " << filePath << "\n";
    return defs;
}

// -------------------------------------------------------------------------
// registerNPC
// -------------------------------------------------------------------------

void ServerNPCManager::registerNPC(uint32_t networkId,
                                   const std::string& scriptType) {
    scripts_[networkId]      = scriptType;
    aiStates_[networkId]     = {};
    luaAIStates_[networkId]  = {};
    pauseTimers_[networkId]  = 0.0f;
}

// -------------------------------------------------------------------------
// setPauseTimer — freeze an NPC's AI for `duration` seconds
// Called by the Lua engine.AI.pause() binding via the callback wired in
// ServerMain so interaction scripts can hold NPCs in place during dialogue.
// -------------------------------------------------------------------------

void ServerNPCManager::setPauseTimer(uint32_t networkId, float duration) {
    if (pauseTimers_.count(networkId)) {
        pauseTimers_[networkId] = duration;
        std::cout << "[ServerNPCManager] NPC " << networkId
                  << " paused for " << duration << " seconds.\n";
    }
}

// -------------------------------------------------------------------------
// setNpcCameraYaw — sync a new facing direction into the NPC's AI state
// Called by the Lua engine.Transform.lookAt() binding so that the AI tick
// on the next frame starts from the updated heading instead of overwriting
// the rotation that lookAt just set.
// -------------------------------------------------------------------------

void ServerNPCManager::setNpcCameraYaw(uint32_t networkId, float yawDegrees) {
    auto ait = aiStates_.find(networkId);
    if (ait != aiStates_.end()) {
        ait->second.cameraYaw = yawDegrees;
    }
    auto lit = luaAIStates_.find(networkId);
    if (lit != luaAIStates_.end()) {
        lit->second.cameraYaw = yawDegrees;
    }
}

// -------------------------------------------------------------------------
// initLua — initialise the Lua scripting engine and load AI scripts
// -------------------------------------------------------------------------

void ServerNPCManager::initLua(const std::string& resourceRoot) {
    luaEngine_.init(resourceRoot);

    // Expose ConfigManager values to Lua's config table so scripts stay
    // data-driven (reads the same world_config.json as C++).

    // Collect unique script paths from registered NPC prefabs.
    // Walk through all registered NPCs, look up their prefab, and load
    // the ai_script file declared in the prefab.
    std::unordered_map<std::string, bool> loaded;

    for (const auto& [nid, scriptType] : scripts_) {
        // Try to find the prefab that corresponds to this NPC's model type.
        // We search all loaded prefabs for a matching "ai_script" → scriptType.
        for (const auto& prefabId : PrefabManager::get().allIds()) {
            const auto& prefab = PrefabManager::get().getPrefab(prefabId);
            // Match by AIComponent.script name
            if (prefab.contains("components") &&
                prefab["components"].contains("AIComponent") &&
                prefab["components"]["AIComponent"].value("script", "") == scriptType) {
                if (prefab.contains("ai_script")) {
                    std::string scriptPath = prefab["ai_script"].get<std::string>();
                    if (!loaded.count(scriptPath)) {
                        luaEngine_.loadScript(scriptPath);
                        loaded[scriptPath] = true;
                    }
                }
                break;
            }
        }
    }

    // Check if any script functions loaded successfully.
    bool anyLoaded = false;
    for (const auto& [nid, scriptType] : scripts_) {
        if (luaEngine_.hasScript(scriptType)) {
            anyLoaded = true;
            break;
        }
    }

    if (anyLoaded) {
        luaReady_ = true;
        std::cout << "[ServerNPCManager] Lua AI scripts loaded — using script-driven AI.\n";
    } else {
        luaReady_ = false;
        std::cout << "[ServerNPCManager] No Lua AI scripts available — using built-in C++ AI.\n";
    }
}

// -------------------------------------------------------------------------
// tick — generate synthetic inputs for every registered NPC
// -------------------------------------------------------------------------

void ServerNPCManager::tick(float dt,
    std::unordered_map<uint32_t, Network::PlayerInputPacket>& outInputs) {

    for (auto& [id, ai] : aiStates_) {
        // ----- NPC AI Pause (set by engine.AI.pause() from Lua scripts) -----
        // Decrement the pause timer and skip input generation while it is active.
        // This lets interaction scripts freeze an NPC in place during dialogue.
        auto pit = pauseTimers_.find(id);
        if (pit != pauseTimers_.end() && pit->second > 0.0f) {
            pit->second -= dt;
            if (pit->second < 0.0f) pit->second = 0.0f;
            // Emit a zero-movement packet so the NPC stands still on the server.
            // Preserve the NPC's current heading so SharedMovement::applyInput does
            // NOT reset rotation.y to 0 (north) — which would immediately undo any
            // lookAt rotation applied by an interaction script.
            Network::PlayerInputPacket idle{};
            idle.deltaTime  = dt;
            idle.cameraYaw  = ai.cameraYaw;
            outInputs[id] = idle;
            continue;
        }
        // --------------------------------------------------------------------

        const auto& script = scripts_[id];

        // Try Lua first if available.
        if (luaReady_ && luaEngine_.hasScript(script)) {
            auto& luaState = luaAIStates_[id];
            auto pkt = luaEngine_.tickAI(script, id, dt, luaState);

            // Sync the Lua AI state back to the C++ NPCAIState for consistency
            // (e.g. if we ever need to inspect it from C++ or fall back mid-run).
            ai.timer     = luaState.timer;
            ai.phase     = luaState.phase;
            ai.cameraYaw = luaState.cameraYaw;

            outInputs[id] = pkt;
        } else {
            // Fallback: use built-in C++ AI.
            Network::PlayerInputPacket inputPacket{};
            inputPacket.deltaTime = dt;

            if (script == "GuardAI") {
                tickGuard(id, ai, dt, inputPacket);
            } else {
                // Default: WanderAI
                tickWander(id, ai, dt, inputPacket);
            }
            outInputs[id] = inputPacket;
        }
    }
}

// -------------------------------------------------------------------------
// WanderAI — walk forward for 3 s, turn for 1 s, repeat
// [Phase 3.2] Updated to use boolean input flags + accumulated cameraYaw.
// NPC turn speed is now read from ConfigManager (data-driven).
// -------------------------------------------------------------------------

void ServerNPCManager::tickWander(uint32_t /*id*/, NPCAIState& ai,
                                  float dt, Network::PlayerInputPacket& out) {
    const float npcTurnSpeed = ConfigManager::get().physics.npcTurnSpeed;
    ai.timer += dt;

    switch (ai.phase) {
        case 0: // Walk forward
            out.moveForward = true;
            if (ai.timer >= 3.0f) { ai.timer = 0.0f; ai.phase = 1; }
            break;
        case 1: // Turn (accumulate yaw)
            ai.cameraYaw += npcTurnSpeed * dt;
            if (ai.timer >= 1.0f) { ai.timer = 0.0f; ai.phase = 0; }
            break;
        default:
            ai.phase = 0;
            ai.timer = 0.0f;
            break;
    }
    out.cameraYaw = ai.cameraYaw;
}

// -------------------------------------------------------------------------
// GuardAI — stand still, occasionally look around
// [Phase 3.2] Updated to use boolean input flags + accumulated cameraYaw.
// NPC turn speed is now read from ConfigManager (data-driven).
// -------------------------------------------------------------------------

void ServerNPCManager::tickGuard(uint32_t /*id*/, NPCAIState& ai,
                                 float dt, Network::PlayerInputPacket& out) {
    const float npcTurnSpeed = ConfigManager::get().physics.npcTurnSpeed;
    ai.timer += dt;

    switch (ai.phase) {
        case 0: // Stand still
            if (ai.timer >= 4.0f) { ai.timer = 0.0f; ai.phase = 1; }
            break;
        case 1: // Look left
            ai.cameraYaw += npcTurnSpeed * dt;
            if (ai.timer >= 0.5f) { ai.timer = 0.0f; ai.phase = 2; }
            break;
        case 2: // Look right
            ai.cameraYaw -= npcTurnSpeed * dt;
            if (ai.timer >= 1.0f) { ai.timer = 0.0f; ai.phase = 0; }
            break;
        default:
            ai.phase = 0;
            ai.timer = 0.0f;
            break;
    }
    out.cameraYaw = ai.cameraYaw;
}

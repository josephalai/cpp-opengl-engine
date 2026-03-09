// src/Server/ServerNPCManager.h
//
// Data-driven NPC loader and AI manager for the headless server.
// Reads NPC definitions from JSON (npcs.json) — the successor to the legacy
// server_npcs.cfg pipe-separated text format — and generates synthetic
// PlayerInputPacket commands each tick based on each NPC's assigned AI
// script type.
//
// [Data-Driven AI] When Lua is available (HAS_LUA), the manager loads .lua
// scripts from src/Resources/scripts/ai/ and calls them each tick.  The C++
// switch-statement logic is retained as a fallback when Lua scripts are not
// found or Lua is not compiled in.
//
// JSON format (src/Resources/npcs.json):
//   {
//     "npcs": [
//       { "npc_id": 1, "prefab": "npc_wanderer", "model_type": "npc_wanderer",
//         "position": { "x": 110.0, "y": 3.0, "z": -70.0 }, "script": "WanderAI" },
//       ...
//     ]
//   }
//
// The legacy loadConfig(path) method is retained for backward compatibility.

#ifndef ENGINE_SERVER_NPC_MANAGER_H
#define ENGINE_SERVER_NPC_MANAGER_H

#include "../Network/NetworkPackets.h"
#include "../Scripting/LuaScriptEngine.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

// -------------------------------------------------------------------------
// Server-side entity state (shared between players and NPCs)
// -------------------------------------------------------------------------
struct ServerEntityState {
    glm::vec3   position  = {};
    glm::vec3   rotation  = {};
    std::string modelType = "player";
    uint32_t    lastProcessedInputSequence = 0;
    bool        isNPC     = false;
};

// -------------------------------------------------------------------------
// NPC definition as read from the configuration / JSON file
// -------------------------------------------------------------------------
struct NPCDefinition {
    uint32_t    npcId      = 0;
    std::string modelType  = "npc";
    glm::vec3   startPos   = {};
    std::string scriptType = "WanderAI";
    std::string prefab;    ///< Prefab name (JSON only; empty for legacy cfg)
};

// -------------------------------------------------------------------------
// Per-NPC runtime AI state
// -------------------------------------------------------------------------
struct NPCAIState {
    float timer      = 0.0f;   ///< Accumulated time in current phase.
    int   phase      = 0;      ///< Current behaviour phase index.
    float cameraYaw  = 0.0f;   ///< [Phase 3.2] Accumulated heading (degrees).
};

// -------------------------------------------------------------------------
// ServerNPCManager
// -------------------------------------------------------------------------
class ServerNPCManager {
public:
    /// Load NPC definitions from a JSON file (npcs.json).
    /// Returns the loaded definitions so the caller can register them in the
    /// entity map.  Each NPC should be assigned a networkId by the caller.
    std::vector<NPCDefinition> loadFromJson(const std::string& filePath);

    /// Load NPC definitions from the legacy pipe-separated .cfg file.
    /// Retained for backward compatibility; prefer loadFromJson for new code.
    std::vector<NPCDefinition> loadConfig(const std::string& filePath);

    /// Register an NPC's networkId so the manager can track its AI state.
    void registerNPC(uint32_t networkId, const std::string& scriptType);

    /// Initialise the Lua scripting engine and load AI scripts.
    /// Call once after all NPCs have been registered.
    void initLua(const std::string& resourceRoot);

    /// Tick the AI for all registered NPCs.  Populates `outInputs` with one
    /// synthetic PlayerInputPacket per NPC keyed by networkId.
    /// If Lua scripts are available, calls them; otherwise falls back to the
    /// built-in C++ AI implementations.
    void tick(float dt,
              std::unordered_map<uint32_t, Network::PlayerInputPacket>& outInputs);

private:
    /// Per-NPC AI runtime data, keyed by networkId.
    std::unordered_map<uint32_t, NPCAIState> aiStates_;

    /// Script type per NPC, keyed by networkId.
    std::unordered_map<uint32_t, std::string> scripts_;

    /// Lua AI state per NPC, keyed by networkId.
    std::unordered_map<uint32_t, LuaAIState> luaAIStates_;

    /// Lua scripting engine instance.
    LuaScriptEngine luaEngine_;

    /// Whether Lua is initialised and at least one script was loaded.
    bool luaReady_ = false;

    // --- C++ fallback AI helpers ---
    void tickWander(uint32_t id, NPCAIState& ai, float dt,
                    Network::PlayerInputPacket& out);
    void tickGuard (uint32_t id, NPCAIState& ai, float dt,
                    Network::PlayerInputPacket& out);
};

#endif // ENGINE_SERVER_NPC_MANAGER_H

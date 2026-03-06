// src/Server/ServerNPCManager.h
//
// Data-driven NPC loader and AI manager for the headless server.
// Reads NPC definitions from a configuration file (server_npcs.cfg) and
// generates synthetic PlayerInputPacket commands each tick based on each
// NPC's assigned AI script type.

#ifndef ENGINE_SERVER_NPC_MANAGER_H
#define ENGINE_SERVER_NPC_MANAGER_H

#include "../Network/NetworkPackets.h"
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
// NPC definition as read from the configuration file
// -------------------------------------------------------------------------
struct NPCDefinition {
    uint32_t    npcId      = 0;
    std::string modelType  = "npc";
    glm::vec3   startPos   = {};
    std::string scriptType = "WanderAI";
};

// -------------------------------------------------------------------------
// Per-NPC runtime AI state
// -------------------------------------------------------------------------
struct NPCAIState {
    float timer      = 0.0f;   ///< Accumulated time in current phase.
    int   phase      = 0;      ///< Current behaviour phase index.
};

// -------------------------------------------------------------------------
// ServerNPCManager
// -------------------------------------------------------------------------
class ServerNPCManager {
public:
    /// Load NPC definitions from the given config file.
    /// Returns the loaded definitions so the caller can register them in the
    /// entity map.  Each NPC should be assigned a networkId by the caller.
    std::vector<NPCDefinition> loadConfig(const std::string& filePath);

    /// Register an NPC's networkId so the manager can track its AI state.
    void registerNPC(uint32_t networkId, const std::string& scriptType);

    /// Tick the AI for all registered NPCs.  Populates `outInputs` with one
    /// synthetic PlayerInputPacket per NPC keyed by networkId.
    void tick(float dt,
              std::unordered_map<uint32_t, Network::PlayerInputPacket>& outInputs);

private:
    /// Per-NPC AI runtime data, keyed by networkId.
    std::unordered_map<uint32_t, NPCAIState> aiStates_;

    /// Script type per NPC, keyed by networkId.
    std::unordered_map<uint32_t, std::string> scripts_;

    // --- AI helpers ---
    void tickWander(uint32_t id, NPCAIState& ai, float dt,
                    Network::PlayerInputPacket& out);
    void tickGuard (uint32_t id, NPCAIState& ai, float dt,
                    Network::PlayerInputPacket& out);
};

#endif // ENGINE_SERVER_NPC_MANAGER_H

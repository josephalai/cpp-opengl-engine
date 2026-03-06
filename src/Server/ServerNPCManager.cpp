// src/Server/ServerNPCManager.cpp

#include "ServerNPCManager.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// -------------------------------------------------------------------------
// loadConfig — parse server_npcs.cfg
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
    scripts_[networkId]  = scriptType;
    aiStates_[networkId] = {};
}

// -------------------------------------------------------------------------
// tick — generate synthetic inputs for every registered NPC
// -------------------------------------------------------------------------

void ServerNPCManager::tick(float dt,
    std::unordered_map<uint32_t, Network::PlayerInputPacket>& outInputs) {

    for (auto& [id, ai] : aiStates_) {
        Network::PlayerInputPacket pkt{};
        pkt.deltaTime = dt;

        const auto& script = scripts_[id];
        if (script == "GuardAI") {
            tickGuard(id, ai, dt, pkt);
        } else {
            // Default: WanderAI
            tickWander(id, ai, dt, pkt);
        }
        outInputs[id] = pkt;
    }
}

// -------------------------------------------------------------------------
// WanderAI — walk forward for 3 s, turn for 1 s, repeat
// -------------------------------------------------------------------------

void ServerNPCManager::tickWander(uint32_t /*id*/, NPCAIState& ai,
                                  float dt, Network::PlayerInputPacket& out) {
    ai.timer += dt;

    switch (ai.phase) {
        case 0: // Walk forward
            out.forward = 1.0f;
            out.turn    = 0.0f;
            if (ai.timer >= 3.0f) { ai.timer = 0.0f; ai.phase = 1; }
            break;
        case 1: // Turn
            out.forward = 0.0f;
            out.turn    = 1.0f;
            if (ai.timer >= 1.0f) { ai.timer = 0.0f; ai.phase = 0; }
            break;
        default:
            ai.phase = 0;
            ai.timer = 0.0f;
            break;
    }
}

// -------------------------------------------------------------------------
// GuardAI — stand still, occasionally look around
// -------------------------------------------------------------------------

void ServerNPCManager::tickGuard(uint32_t /*id*/, NPCAIState& ai,
                                 float dt, Network::PlayerInputPacket& out) {
    ai.timer += dt;

    switch (ai.phase) {
        case 0: // Stand still
            out.forward = 0.0f;
            out.turn    = 0.0f;
            if (ai.timer >= 4.0f) { ai.timer = 0.0f; ai.phase = 1; }
            break;
        case 1: // Look left
            out.forward = 0.0f;
            out.turn    = 1.0f;
            if (ai.timer >= 0.5f) { ai.timer = 0.0f; ai.phase = 2; }
            break;
        case 2: // Look right
            out.forward = 0.0f;
            out.turn    = -1.0f;
            if (ai.timer >= 1.0f) { ai.timer = 0.0f; ai.phase = 0; }
            break;
        default:
            ai.phase = 0;
            ai.timer = 0.0f;
            break;
    }
}

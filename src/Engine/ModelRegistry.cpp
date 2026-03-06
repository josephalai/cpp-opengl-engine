// src/Engine/ModelRegistry.cpp

#include "ModelRegistry.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// Static registry storage
std::unordered_map<std::string, ModelRegistryEntry> ModelRegistry::registry_;

// ---------------------------------------------------------------------------
// Helper: trim leading/trailing whitespace from a string
// ---------------------------------------------------------------------------
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
// Helper: parse a "x,y,z" token into a glm::vec3
// ---------------------------------------------------------------------------
static glm::vec3 parseVec3(const std::string& token) {
    glm::vec3 v{0.0f};
    char sep;
    std::istringstream ss(token);
    ss >> v.x >> sep >> v.y >> sep >> v.z;
    return v;
}

// ---------------------------------------------------------------------------
// loadFromFile
// ---------------------------------------------------------------------------
bool ModelRegistry::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ModelRegistry] Could not open: " << path << "\n";
        return false;
    }

    int loaded = 0;
    std::string line;
    while (std::getline(file, line)) {
        // Strip comments and skip blank lines
        auto commentPos = line.find('#');
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);
        line = trim(line);
        if (line.empty()) continue;

        // Split by '|'
        std::vector<std::string> fields;
        {
            std::istringstream ss(line);
            std::string field;
            while (std::getline(ss, field, '|'))
                fields.push_back(trim(field));
        }

        // Expect exactly 7 fields:
        // key | loader_type | asset_path | texture | scale | rot_x,rot_y,rot_z | offset_x,offset_y,offset_z
        if (fields.size() < 7) {
            std::cerr << "[ModelRegistry] Skipping malformed line: " << line << "\n";
            continue;
        }

        ModelRegistryEntry entry;
        entry.key = fields[0];
        if (entry.key.empty()) continue;

        // Loader type
        std::string lt = fields[1];
        std::transform(lt.begin(), lt.end(), lt.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        entry.loaderType = (lt == "animated")
                           ? ModelRegistryEntry::LoaderType::ANIMATED
                           : ModelRegistryEntry::LoaderType::OBJ;

        entry.assetPath   = fields[2];
        entry.textureName = (fields[3] == "-") ? "" : fields[3];

        // Scale
        try { entry.scale = std::stof(fields[4]); }
        catch (...) { entry.scale = 1.0f; }

        // Rotation
        entry.rotation   = parseVec3(fields[5]);

        // Offset
        entry.modelOffset = parseVec3(fields[6]);

        registry_[entry.key] = entry;
        ++loaded;
        std::cout << "[ModelRegistry] Registered '" << entry.key << "' ("
                  << (entry.loaderType == ModelRegistryEntry::LoaderType::ANIMATED
                      ? "animated" : "obj")
                  << ", " << entry.assetPath << ")\n";
    }

    std::cout << "[ModelRegistry] Loaded " << loaded << " entries from " << path << "\n";
    return loaded > 0;
}

// ---------------------------------------------------------------------------
// lookup
// ---------------------------------------------------------------------------
const ModelRegistryEntry* ModelRegistry::lookup(const std::string& modelType) {
    auto it = registry_.find(modelType);
    if (it != registry_.end()) return &it->second;
    return nullptr;
}

// ---------------------------------------------------------------------------
// registerDefaults — hardcoded fallbacks for when models.cfg is absent
// ---------------------------------------------------------------------------
void ModelRegistry::registerDefaults() {
    // Player / remote player character — animated .glb
    {
        ModelRegistryEntry e;
        e.key        = "player";
        e.loaderType = ModelRegistryEntry::LoaderType::ANIMATED;
        e.assetPath  = "Characters/characters.glb";
        e.scale      = 1.0f;
        e.rotation   = {-90.0f, 0.0f, 0.0f};
        e.modelOffset = {0.0f, 1.445f, 0.0f};
        registry_[e.key] = e;
    }
    // NPC wanderer — same model as player
    {
        ModelRegistryEntry e;
        e.key        = "npc_wanderer";
        e.loaderType = ModelRegistryEntry::LoaderType::ANIMATED;
        e.assetPath  = "Characters/characters.glb";
        e.scale      = 1.0f;
        e.rotation   = {-90.0f, 0.0f, 0.0f};
        e.modelOffset = {0.0f, 1.445f, 0.0f};
        registry_[e.key] = e;
    }
    // NPC guard — static OBJ lamp mesh as placeholder
    {
        ModelRegistryEntry e;
        e.key         = "npc_guard";
        e.loaderType  = ModelRegistryEntry::LoaderType::OBJ;
        e.assetPath   = "lamp";
        e.textureName = "lamp";
        e.scale       = 1.0f;
        registry_[e.key] = e;
    }
    std::cout << "[ModelRegistry] Registered " << registry_.size()
              << " default entries.\n";
}

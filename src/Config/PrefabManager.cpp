// src/Config/PrefabManager.cpp

#include "PrefabManager.h"

#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// -------------------------------------------------------------------------
// Singleton
// -------------------------------------------------------------------------

PrefabManager& PrefabManager::get() {
    static PrefabManager instance;
    return instance;
}

// -------------------------------------------------------------------------
// loadAll — recursively scan the prefabs directory for JSON files
// -------------------------------------------------------------------------

void PrefabManager::loadAll(const std::string& resourceRoot) {
    const std::string prefabDir = resourceRoot + "/src/Resources/prefabs";

    if (!fs::exists(prefabDir) || !fs::is_directory(prefabDir)) {
        std::cerr << "[PrefabManager] Prefabs directory not found: "
                  << prefabDir << "\n";
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(prefabDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        loadFile(entry.path().string());
    }

    std::cout << "[PrefabManager] Loaded " << prefabs_.size()
              << " prefab(s) from " << prefabDir << "\n";
}

// -------------------------------------------------------------------------
// loadFile — parse a single JSON prefab and insert into the cache
// -------------------------------------------------------------------------

void PrefabManager::loadFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[PrefabManager] Could not open " << filePath << "\n";
        return;
    }

    nlohmann::json root;
    try { file >> root; }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[PrefabManager] JSON parse error in " << filePath
                  << ": " << e.what() << "\n";
        return;
    }

    // Determine the prefab ID.  Prefer the explicit "id" field; fall back to
    // the filename stem (e.g. "npc_guard" from "npc_guard.json").
    std::string id;
    if (root.contains("id") && root["id"].is_string()) {
        id = root["id"].get<std::string>();
    } else {
        id = fs::path(filePath).stem().string();
    }

    prefabs_[id] = std::move(root);
}

// -------------------------------------------------------------------------
// getPrefab
// -------------------------------------------------------------------------

const nlohmann::json& PrefabManager::getPrefab(const std::string& id) const {
    auto it = prefabs_.find(id);
    if (it != prefabs_.end()) return it->second;
    static const nlohmann::json kEmpty;
    return kEmpty;
}

// -------------------------------------------------------------------------
// hasPrefab
// -------------------------------------------------------------------------

bool PrefabManager::hasPrefab(const std::string& id) const {
    return prefabs_.count(id) > 0;
}

// -------------------------------------------------------------------------
// allIds
// -------------------------------------------------------------------------

std::vector<std::string> PrefabManager::allIds() const {
    std::vector<std::string> ids;
    ids.reserve(prefabs_.size());
    for (const auto& [k, _] : prefabs_) ids.push_back(k);
    return ids;
}

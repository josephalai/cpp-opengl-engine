// src/Config/PrefabManager.cpp

#include "PrefabManager.h"

#include "../Util/FileSystem.h"

#include <iostream>
#include <filesystem>
#include <cmath>

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
    auto bytes = FileSystem::readAllBytes(filePath);
    if (bytes.empty()) {
        std::cerr << "[PrefabManager] Could not open " << filePath << "\n";
        return;
    }

    nlohmann::json root;
    try { root = nlohmann::json::parse(bytes.begin(), bytes.end()); }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[PrefabManager] JSON parse error in " << filePath
                  << ": " << e.what() << "\n";
        return;
    }

    std::cout << "[PrefabManager::loadFile] Parsed '" << filePath << "'.\n";

    // Determine the prefab ID.  Prefer the explicit "id" field; fall back to
    // the filename stem (e.g. "npc_guard" from "npc_guard.json").
    std::string id;
    if (root.contains("id") && root["id"].is_string()) {
        id = root["id"].get<std::string>();
    } else {
        id = fs::path(filePath).stem().string();
    }

    prefabs_[id] = std::move(root);

    std::cout << "[PrefabManager::loadFile]   Registered prefab id='" << id << "'"
              << (prefabs_[id].contains("id") ? " (explicit)" : " (from filename)")
              << ", animated=" << prefabs_[id].value("animated", false)
              << ", has mesh=" << prefabs_[id].contains("mesh")
              << ".\n";
}

// -------------------------------------------------------------------------
// getPrefab
// -------------------------------------------------------------------------

const nlohmann::json& PrefabManager::getPrefab(const std::string& id) const {
    auto it = prefabs_.find(id);
    if (it != prefabs_.end()) return it->second;
    static const nlohmann::json kEmpty;
    std::cerr << "[PrefabManager::getPrefab] Prefab '" << id << "' not found.\n";
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

// -------------------------------------------------------------------------
// setMeshAABB / getMeshHalfExtentsXZ
// -------------------------------------------------------------------------

void PrefabManager::setMeshAABB(const std::string& id,
                                 const glm::vec3& min,
                                 const glm::vec3& max) {
    meshAABBs_[id] = { min, max, true };
}

glm::vec2 PrefabManager::getMeshHalfExtentsXZ(const std::string& id, float scale) const {
    auto it = meshAABBs_.find(id);
    if (it != meshAABBs_.end() && it->second.valid) {
        const auto& aabb = it->second;
        float hx = std::abs(aabb.max.x - aabb.min.x) * 0.5f * scale;
        float hz = std::abs(aabb.max.z - aabb.min.z) * 0.5f * scale;
        return { hx, hz };
    }
    return { -1.0f, -1.0f };
}

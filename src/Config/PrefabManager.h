// src/Config/PrefabManager.h
//
// Prefab Registry — loads every JSON file from the /prefabs/ directory on
// boot and stores them in a fast lookup map.  Any system can retrieve a
// prefab definition by its string ID without knowing the filesystem layout.
//
// Usage:
//   PrefabManager::get().loadAll(resourceRoot);        // once at boot
//   const auto& j = PrefabManager::get().getPrefab("npc_guard");

#ifndef ENGINE_PREFAB_MANAGER_H
#define ENGINE_PREFAB_MANAGER_H

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class PrefabManager {
public:
    /// Singleton accessor.
    static PrefabManager& get();

    /// Recursively scan the prefabs directory under the given resource root
    /// and cache every JSON file.  The prefab ID is taken from the "id" field
    /// inside the JSON (falls back to the filename without extension).
    void loadAll(const std::string& resourceRoot);

    /// Load a single prefab JSON file and add it to the cache.
    void loadFile(const std::string& filePath);

    /// Look up a prefab by its string ID.
    /// Returns a reference to the cached JSON or an empty static JSON if not found.
    const nlohmann::json& getPrefab(const std::string& id) const;

    /// Check whether a prefab ID is registered.
    bool hasPrefab(const std::string& id) const;

    /// Return all loaded prefab IDs (for debugging / iteration).
    std::vector<std::string> allIds() const;

private:
    PrefabManager() = default;
    PrefabManager(const PrefabManager&) = delete;
    PrefabManager& operator=(const PrefabManager&) = delete;

    std::unordered_map<std::string, nlohmann::json> prefabs_;
};

#endif // ENGINE_PREFAB_MANAGER_H

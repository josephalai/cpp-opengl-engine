// src/Engine/ModelRegistry.h
// Maps model-type string keys (e.g. "player", "npc_guard") to the loading
// instructions needed to instantiate a remote entity on the client.
// The registry is populated from models.cfg at startup and provides fallback
// defaults when the config file is absent.

#ifndef ENGINE_MODELREGISTRY_H
#define ENGINE_MODELREGISTRY_H

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

/// Metadata for one model type understood by the spawn pipeline.
struct ModelRegistryEntry {
    enum class LoaderType { OBJ, ANIMATED };

    std::string key;                             ///< Unique model-type key
    LoaderType  loaderType  = LoaderType::OBJ;  ///< Which loader to use
    std::string assetPath;                       ///< Relative to Tutorial/ (animated) or OBJ name (obj)
    std::string textureName;                     ///< Texture name for OBJ models; empty for animated
    float       scale       = 1.0f;
    glm::vec3   rotation    = {0.0f, 0.0f, 0.0f};  ///< Euler rotation in degrees (baked into model)
    glm::vec3   modelOffset = {0.0f, 0.0f, 0.0f};  ///< Visual-only world-space offset
};

/// Singleton-style static registry that maps model type strings to
/// ModelRegistryEntry loading metadata.
class ModelRegistry {
public:
    /// Load registry entries from a pipe-delimited config file.
    /// Format: key | loader_type | asset_path | texture | scale | rx,ry,rz | ox,oy,oz
    /// Returns true if at least one entry was loaded successfully.
    static bool loadFromFile(const std::string& path);

    /// Look up a model type by key.  Returns nullptr if not registered.
    static const ModelRegistryEntry* lookup(const std::string& modelType);

    /// Register hardcoded fallback entries used when models.cfg is absent.
    static void registerDefaults();

private:
    static std::unordered_map<std::string, ModelRegistryEntry> registry_;
};

#endif // ENGINE_MODELREGISTRY_H

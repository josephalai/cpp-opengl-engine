// src/ModelLoader/ModelCache.h
// Caches loaded AnimatedModel objects by file path.

#ifndef ENGINE_MODELCACHE_H
#define ENGINE_MODELCACHE_H

#include <string>
#include <unordered_map>
#include <vector>
#include "../Animation/AnimatedModel.h"

class ModelCache {
public:
    /// Return cached model, or load and cache it if not yet loaded.
    /// Returns nullptr on load failure.
    AnimatedModel* getModel(const std::string& path);

    /// Pre-load a list of model paths (best-effort; failures are logged).
    void preload(const std::vector<std::string>& paths);

    /// Release all cached models.
    void clear();

    ~ModelCache() { clear(); }

private:
    std::unordered_map<std::string, AnimatedModel*> cache;
};

#endif // ENGINE_MODELCACHE_H

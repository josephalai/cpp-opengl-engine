// src/ModelLoader/ModelCache.cpp

#include "ModelCache.h"
#include "../Animation/AnimationLoader.h"
#include <iostream>

AnimatedModel* ModelCache::getModel(const std::string& path) {
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;

    AnimatedModel* model = AnimationLoader::load(path);
    if (!model) {
        std::cerr << "[ModelCache] Failed to load: " << path << "\n";
    }
    cache[path] = model;  // Cache even nullptr to avoid repeated failures
    return model;
}

void ModelCache::preload(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        getModel(p);  // Side-effect: inserts into cache
    }
}

void ModelCache::clear() {
    for (auto& [path, model] : cache) {
        delete model;
    }
    cache.clear();
}

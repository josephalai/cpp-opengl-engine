// src/Engine/ResourceManager.cpp

#include "ResourceManager.h"
#include "../Animation/AnimatedModel.h"
#include "../Animation/AnimationLoader.h"
#include <iostream>

ResourceHandle<AnimatedModel>
ResourceManager::loadAnimatedModel(const std::string& path) {
    // Return cached handle if already loaded
    auto it = animatedModelPaths_.find(path);
    if (it != animatedModelPaths_.end()) {
        return ResourceHandle<AnimatedModel>(it->second);
    }

    AnimatedModel* model = AnimationLoader::load(path);
    if (!model) {
        std::cerr << "[ResourceManager] Failed to load animated model: " << path << "\n";
        return ResourceHandle<AnimatedModel>();
    }

    uint32_t id = allocId();
    animatedModels_[id]       = model;
    animatedModelPaths_[path] = id;
    return ResourceHandle<AnimatedModel>(id);
}

AnimatedModel*
ResourceManager::resolve(ResourceHandle<AnimatedModel> handle) {
    if (!handle.isValid()) return nullptr;
    auto it = animatedModels_.find(handle.rawId());
    return it != animatedModels_.end() ? it->second : nullptr;
}

void ResourceManager::unloadAll() {
    for (auto& [id, model] : animatedModels_) {
        if (model) {
            model->cleanUp();
            delete model;
        }
    }
    animatedModels_.clear();
    animatedModelPaths_.clear();
    nextId_ = 1;
}

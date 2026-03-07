// src/Engine/ResourceManager.cpp

#include "ResourceManager.h"
#include "AsyncResourceLoader.h"
#include "../Animation/AnimatedModel.h"
#include "../Animation/AnimationLoader.h"
#include <iostream>

ResourceHandle<AnimatedModel>
ResourceManager::loadAnimatedModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Convert path to StringId for O(1) cache lookup
    StringId pathId(path);
    auto it = animatedModelPaths_.find(pathId);
    if (it != animatedModelPaths_.end()) {
        return ResourceHandle<AnimatedModel>(it->second);
    }

    AnimatedModel* model = AnimationLoader::load(path);
    if (!model) {
        std::cerr << "[ResourceManager] Failed to load animated model: " << path << "\n";
        return ResourceHandle<AnimatedModel>();
    }

    uint32_t id = allocId();
    animatedModels_[id]         = model;
    animatedModelPaths_[pathId] = id;
    return ResourceHandle<AnimatedModel>(id);
}

void ResourceManager::loadAnimatedModelAsync(
        const std::string& path,
        std::function<void(ResourceHandle<AnimatedModel>)> callback) {

    // Check the cache first (under lock) and short-circuit if already loaded.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        StringId pathId(path);
        auto it = animatedModelPaths_.find(pathId);
        if (it != animatedModelPaths_.end()) {
            callback(ResourceHandle<AnimatedModel>(it->second));
            return;
        }
    }

    AsyncResourceLoader::instance().loadAnimatedModelAsync(
        path,
        [this, path, callback](AnimatedModel* model) {
            // Invoked on the main thread by GLUploadQueue::processAll().
            if (!model) {
                std::cerr << "[ResourceManager] Async load failed: " << path << "\n";
                callback(ResourceHandle<AnimatedModel>());
                return;
            }
            uint32_t id;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                StringId pathId(path);
                // Another async request may have loaded it first.
                auto it = animatedModelPaths_.find(pathId);
                if (it != animatedModelPaths_.end()) {
                    model->cleanUp();
                    delete model;
                    callback(ResourceHandle<AnimatedModel>(it->second));
                    return;
                }
                id = allocId();
                animatedModels_[id]         = model;
                animatedModelPaths_[pathId] = id;
            }
            callback(ResourceHandle<AnimatedModel>(id));
        });
}

AnimatedModel*
ResourceManager::resolve(ResourceHandle<AnimatedModel> handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!handle.isValid()) return nullptr;
    auto it = animatedModels_.find(handle.rawId());
    return it != animatedModels_.end() ? it->second : nullptr;
}

void ResourceManager::unloadAll() {
    std::lock_guard<std::mutex> lock(mutex_);
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

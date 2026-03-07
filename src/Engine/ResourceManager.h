// src/Engine/ResourceManager.h
// Handle-based resource manager.
// Provides type-safe handles that decouple resource loading from usage,
// enabling safe lifecycle management and future zone-based unloading.
//
// Usage:
//   ResourceHandle<AnimatedModel> h = mgr.loadAnimatedModel("path/to/model.fbx");
//   AnimatedModel* ptr = mgr.resolve(h);   // nullptr if unloaded
//   mgr.unloadAll();

#ifndef ENGINE_RESOURCEMANAGER_H
#define ENGINE_RESOURCEMANAGER_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "StringId.h"

// Forward declarations for the managed types
class AnimatedModel;
class TexturedModel;

// ---------------------------------------------------------------------------
// ResourceHandle<T> — lightweight, copyable opaque handle.
// ---------------------------------------------------------------------------
template<typename T>
class ResourceHandle {
public:
    constexpr ResourceHandle() : id_(0) {}
    explicit constexpr ResourceHandle(uint32_t id) : id_(id) {}

    bool isValid()    const { return id_ != 0; }
    uint32_t rawId()  const { return id_; }

    bool operator==(const ResourceHandle& o) const { return id_ == o.id_; }
    bool operator!=(const ResourceHandle& o) const { return id_ != o.id_; }

private:
    uint32_t id_;
};

// ---------------------------------------------------------------------------
// ResourceManager — central registry for engine resources.
// ---------------------------------------------------------------------------
class ResourceManager {
public:
    /// Load (or return cached) animated model from path (synchronous).
    ResourceHandle<AnimatedModel> loadAnimatedModel(const std::string& path);

    /// Load an animated model asynchronously.
    /// Parsing happens on a background thread; GL upload and callback are
    /// invoked on the main thread via GLUploadQueue.
    void loadAnimatedModelAsync(const std::string& path,
                                std::function<void(ResourceHandle<AnimatedModel>)> callback);

    /// Resolve a handle to a raw pointer (nullptr if not found / unloaded).
    AnimatedModel* resolve(ResourceHandle<AnimatedModel> handle);

    /// Unload all managed resources and free their memory.
    void unloadAll();

private:
    uint32_t nextId_ = 1;
    std::mutex mutex_;

    std::unordered_map<uint32_t, AnimatedModel*> animatedModels_;
    // StringId-keyed path cache: O(1) integer-comparison lookup on hot paths.
    std::unordered_map<StringId, uint32_t>       animatedModelPaths_;

    uint32_t allocId() { return nextId_++; }
};

#endif // ENGINE_RESOURCEMANAGER_H

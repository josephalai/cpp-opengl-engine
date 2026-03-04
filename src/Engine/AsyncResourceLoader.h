// src/Engine/AsyncResourceLoader.h
// Asynchronous resource loading pipeline.
//
// File I/O and Assimp parsing happen on background worker threads.
// OpenGL API calls are enqueued to GLUploadQueue and executed on the main
// thread inside Engine::run() via GLUploadQueue::instance().processAll().

#ifndef ENGINE_ASYNCRESOURCELOADER_H
#define ENGINE_ASYNCRESOURCELOADER_H

#include <string>
#include <functional>

class AnimatedModel;

class AsyncResourceLoader {
public:
    /// Singleton accessor.
    static AsyncResourceLoader& instance();

    /// Load an animated model asynchronously.
    /// The Assimp parse + stbi texture decode happen on a worker thread.
    /// GL object creation (VAO/VBO/EBO/textures) is deferred to the main thread
    /// via GLUploadQueue.  callback is invoked on the main thread with the
    /// finished AnimatedModel* (nullptr on failure).
    void loadAnimatedModelAsync(const std::string& path,
                                std::function<void(AnimatedModel*)> callback);

    /// Load a 2-D texture asynchronously.
    /// stbi_load happens on a worker thread; glGenTextures/glTexImage2D are
    /// executed on the main thread.  callback receives the GL texture ID (0 on failure).
    void loadTextureAsync(const std::string& path,
                          std::function<void(unsigned int)> callback);

    /// Block until all in-flight worker-thread jobs have finished.
    /// Call this during engine shutdown before destroying the GL context.
    void shutdown();

private:
    AsyncResourceLoader()  = default;
    ~AsyncResourceLoader() = default;
};

#endif // ENGINE_ASYNCRESOURCELOADER_H

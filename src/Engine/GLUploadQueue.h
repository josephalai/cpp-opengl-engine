// src/Engine/GLUploadQueue.h
// Thread-safe queue for OpenGL upload tasks.
// Background threads push lambdas; the main thread drains them each frame.

#ifndef ENGINE_GLUPLOADQUEUE_H
#define ENGINE_GLUPLOADQUEUE_H

#include <functional>
#include <queue>
#include <mutex>

class GLUploadQueue {
public:
    /// Singleton accessor — the queue must only be drained on the GL thread.
    static GLUploadQueue& instance();

    /// Push a GL-work lambda.  Safe to call from any thread.
    void enqueue(std::function<void()> task);

    /// Drain up to maxPerFrame pending tasks on the calling (GL) thread.
    /// Pass 0 to process all pending tasks in one call.
    void processAll(int maxPerFrame = 0);

    /// Returns true when no tasks are pending.
    bool empty() const;

private:
    GLUploadQueue()  = default;
    ~GLUploadQueue() = default;

    mutable std::mutex                   mutex_;
    std::queue<std::function<void()>>    queue_;
};

#endif // ENGINE_GLUPLOADQUEUE_H

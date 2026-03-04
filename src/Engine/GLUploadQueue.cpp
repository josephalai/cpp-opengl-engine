// src/Engine/GLUploadQueue.cpp

#include "GLUploadQueue.h"

GLUploadQueue& GLUploadQueue::instance() {
    static GLUploadQueue inst;
    return inst;
}

void GLUploadQueue::enqueue(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(task));
}

void GLUploadQueue::processAll(int maxPerFrame) {
    // Swap the pending queue into a local list under the lock, then execute
    // outside the lock so background threads can still enqueue concurrently.
    std::queue<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (maxPerFrame <= 0 || static_cast<int>(queue_.size()) <= maxPerFrame) {
            std::swap(local, queue_);
        } else {
            for (int i = 0; i < maxPerFrame; ++i) {
                local.push(std::move(queue_.front()));
                queue_.pop();
            }
        }
    }

    while (!local.empty()) {
        local.front()();
        local.pop();
    }
}

bool GLUploadQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

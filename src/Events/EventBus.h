// src/Events/EventBus.h
// Thread-safe, template-based Pub/Sub event dispatcher (singleton).
//
// Usage — subscribe at init time:
//   EventBus::instance().subscribe<MyEvent>(
//       [](const MyEvent& e) { /* handle e */ });
//
// Usage — publish from any system:
//   EventBus::instance().publish(MyEvent{...});
//
// Handlers are invoked synchronously on the publishing thread in the order
// they were registered.  A snapshot of the handler list is captured under
// the mutex before iteration, so handlers may themselves call subscribe() or
// publish() without causing a deadlock or invalidating the iterator.
//
// Thread safety: subscribe() and publish() are both safe to call from
// multiple threads simultaneously.  Handler invocations are not serialised
// with respect to each other once the snapshot is taken.

#ifndef ENGINE_EVENTBUS_H
#define ENGINE_EVENTBUS_H

#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

class EventBus {
public:
    /// Returns the process-wide EventBus singleton.
    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

    /// Subscribe to events of type T.
    /// The callable is stored as a type-erased std::function and invoked for
    /// every matching publish() call for the lifetime of the EventBus.
    template<typename T>
    void subscribe(std::function<void(const T&)> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[std::type_index(typeid(T))].push_back(
            [h = std::move(handler)](const void* evt) {
                h(*static_cast<const T*>(evt));
            });
    }

    /// Publish an event to all registered subscribers of type T.
    /// Handlers are called synchronously before publish() returns.
    template<typename T>
    void publish(const T& event) {
        // Take a snapshot so handlers can call subscribe()/publish() safely.
        std::vector<std::function<void(const void*)>> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(std::type_index(typeid(T)));
            if (it != handlers_.end()) {
                snapshot = it->second;
            }
        }
        for (const auto& h : snapshot) {
            h(&event);
        }
    }

    /// Remove all subscribers for all event types.
    /// Useful in tests or when tearing down and re-initialising subsystems.
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }

private:
    EventBus() = default;
    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;

    std::mutex mutex_;
    std::unordered_map<
        std::type_index,
        std::vector<std::function<void(const void*)>>> handlers_;
};

#endif // ENGINE_EVENTBUS_H

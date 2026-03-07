// src/Entities/ComponentPool.h
//
// MMORPG Architecture — Phase 1, Step 2: Custom Memory Allocators.
//
// ComponentPool<T> provides a contiguous pre-allocated slab of component
// objects.  Instead of scattering components randomly across the heap via
// make_unique, the pool hands out slots from a single vector.  When the
// server iterates over thousands of NetworkSyncComponents to broadcast
// transforms, all data resides in one contiguous array — CPU-cache-friendly
// and free of per-object heap-allocation overhead.
//
// Usage:
//   auto* comp = ComponentPool<NetworkSyncComponent>::global().allocate();
//   // ... use comp ...
//   ComponentPool<NetworkSyncComponent>::global().release(comp);
//
// Pointer stability guarantee:
//   The pool reserves kInitialCapacity slots upfront.  Pointers returned by
//   allocate() remain valid as long as the total number of live allocations
//   stays below the current capacity.  The pool doubles its capacity when
//   needed; a reallocation invalidates all existing raw pointers, so callers
//   must not cache raw component pointers across potential-realloc points.
//   Entity::addComponent stores the pointer alongside a release callback, so
//   the entity always releases via the stored pointer — safe because Entity
//   lifetime is shorter than Pool lifetime.

#ifndef ENGINE_COMPONENTPOOL_H
#define ENGINE_COMPONENTPOOL_H

#include <vector>
#include <cstddef>
#include <cassert>
#include <iostream>

template<typename T>
class ComponentPool {
public:
    /// Default initial capacity (number of pre-allocated component slots).
    static constexpr size_t kInitialCapacity = 4096;

    ComponentPool() {
        storage_.reserve(kInitialCapacity);
        active_.reserve(kInitialCapacity);
    }

    /// Non-copyable, non-movable singleton.
    ComponentPool(const ComponentPool&)            = delete;
    ComponentPool& operator=(const ComponentPool&) = delete;

    // -----------------------------------------------------------------------
    // Global singleton accessor — one pool per component type.
    // -----------------------------------------------------------------------
    static ComponentPool<T>& global() {
        static ComponentPool<T> instance;
        return instance;
    }

    // -----------------------------------------------------------------------
    // allocate — obtain a default-constructed component slot.
    //
    // If a free slot is available it is reused (T is default-constructed in
    // place via assignment).  Otherwise a new slot is appended.
    //
    // WARNING: if the internal vector must grow beyond its current capacity,
    // all existing raw pointers are invalidated.  The pool logs a warning and
    // doubles capacity to minimise future reallocations.
    // -----------------------------------------------------------------------
    T* allocate() {
        if (!free_.empty()) {
            size_t idx = free_.back();
            free_.pop_back();
            storage_[idx] = T{};    // reset to default state
            active_[idx]  = true;
            return &storage_[idx];
        }

        if (storage_.size() == storage_.capacity()) {
            // Capacity exhausted — double it and warn.
            size_t newCap = storage_.capacity() * 2;
            std::cerr << "[ComponentPool] Capacity exceeded for "
                      << typeid(T).name()
                      << " — reallocating from " << storage_.capacity()
                      << " to " << newCap << " slots. "
                         "Existing raw pointers are INVALIDATED.\n";
            storage_.reserve(newCap);
            active_.reserve(newCap);
        }

        storage_.push_back(T{});
        active_.push_back(true);
        return &storage_.back();
    }

    // -----------------------------------------------------------------------
    // release — return a slot to the free list.
    //
    // ptr must be a pointer previously returned by allocate() on this pool.
    // -----------------------------------------------------------------------
    void release(T* ptr) {
        if (!ptr) return;
        size_t idx = static_cast<size_t>(ptr - storage_.data());
        assert(idx < storage_.size() && active_[idx]);
        active_[idx] = false;
        free_.push_back(idx);
    }

    // -----------------------------------------------------------------------
    // Introspection helpers
    // -----------------------------------------------------------------------
    size_t capacity()  const { return storage_.capacity(); }
    size_t size()      const { return storage_.size() - free_.size(); }
    size_t freeCount() const { return free_.size(); }

    /// Iterate over every *active* component — useful for server-tick loops.
    template<typename Fn>
    void forEach(Fn&& fn) {
        for (size_t i = 0; i < storage_.size(); ++i) {
            if (active_[i]) fn(storage_[i]);
        }
    }

private:
    std::vector<T>      storage_;   ///< Contiguous component slab.
    std::vector<bool>   active_;    ///< Slot occupancy bitfield.
    std::vector<size_t> free_;      ///< Indices of released (reusable) slots.
};

#endif // ENGINE_COMPONENTPOOL_H

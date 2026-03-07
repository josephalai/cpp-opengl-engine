#ifndef ECS_NETWORKSYNCDATA_H
#define ECS_NETWORKSYNCDATA_H

#include <deque>
#include <cstddef>
#include <glm/glm.hpp>
#include "../../Network/NetworkPackets.h"

/// Pure POD component — holds all per-entity network interpolation state.
/// Replaces the old NetworkSyncComponent (IComponent subclass) in the ECS migration.
/// A future NetworkInterpolationSystem will query
/// registry.view<TransformComponent, NetworkSyncData>() each frame.
struct NetworkSyncData {
    // --- Tuning (JSON-overridable) ---
    float       interpolationDelay = 0.20f;   ///< seconds of playback lag
    std::size_t maxBufferSize      = 20;       ///< snapshot ring-buffer capacity

    // --- Snapshot buffer ---
    std::deque<Network::TransformSnapshot> buffer;

    // --- Playback clock ---
    float renderTime = 0.0f;   ///< monotonically advancing render-playback clock (seconds)
    bool  started    = false;  ///< true once ≥2 snapshots received and clock synced

    // --- Speed computation (for animation blending) ---
    glm::vec3 previousPosition            = glm::vec3(0.0f);
    bool      previousPositionInitialized = false;
    float     currentSpeed                = 0.0f;  ///< most recent XZ speed (units/sec)
};

#endif // ECS_NETWORKSYNCDATA_H

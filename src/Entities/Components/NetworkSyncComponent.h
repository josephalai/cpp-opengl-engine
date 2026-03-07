// src/Entities/Components/NetworkSyncComponent.h
//
// Component that drives an Entity's position and rotation from an incoming
// stream of authoritative server snapshots (TransformSnapshot packets).
//
// KEY CONCEPT — Entity Interpolation:
//   Remote entities do NOT snap directly to the newest snapshot.  Instead, the
//   component maintains a small playback clock that lags kInterpolationDelay
//   seconds behind the latest received snapshot.  Each frame it finds the two
//   snapshots that bracket the current playback time and uses glm::mix (LERP)
//   for position and glm::slerp (quaternion) for rotation to produce smooth,
//   stutter-free movement even at a 10 Hz server tick rate.
//
// Edge cases handled:
//   • Buffer empty        → entity is not moved (safe no-op).
//   • Only 1 snapshot     → entity is held at that position.
//   • Buffer starvation   → velocity-based dead reckoning from the last two
//                           snapshots (capped at ~1 tick to avoid runaway drift).
//   • Normal interpolation→ smooth LERP/SLERP between bracketing snapshots.

#ifndef ENGINE_NETWORKSYNCCOMPONENT_H
#define ENGINE_NETWORKSYNCCOMPONENT_H

#include "IComponent.h"
#include "../../Network/NetworkPackets.h"
#include <deque>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

class NetworkSyncComponent : public IComponent {
public:
    // -------------------------------------------------------------------------
    // IComponent interface
    // -------------------------------------------------------------------------
    void init()   override {}
    void update(float deltaTime) override;

    /// JSON initialisation — load tuning parameters from a prefab.
    /// Supported keys:
    ///   "interpolation_delay" (float) — seconds of playback lag
    ///   "max_buffer_size"    (int)    — snapshot ring-buffer capacity
    void initFromJson(const nlohmann::json& j) override;

    // -------------------------------------------------------------------------
    // Server interface
    // -------------------------------------------------------------------------

    /// Push a new authoritative snapshot into the incoming buffer.
    /// Called by MockServer (or the real transport layer in Phase 2+) whenever a
    /// packet arrives.  Thread-safety: all calls must be from the main thread in
    /// Phase 1; Phase 2 will need a mutex here.
    void pushSnapshot(const Network::TransformSnapshot& snapshot);

    /// Returns the most recently computed XZ movement speed (units/sec).
    /// Computed each frame after position is applied; used by AnimationSystem
    /// to drive Walk/Run/Idle transitions for remote characters.
    float getCurrentSpeed() const { return currentSpeed_; }

    // -------------------------------------------------------------------------
    // Tuning constants (defaults; may be overridden via initFromJson)
    // -------------------------------------------------------------------------

    /// How far behind the most-recent snapshot the render clock trails (seconds).
    /// A value of 2× the server tick interval guarantees we almost always have
    /// two bracketing snapshots available for smooth interpolation.
    static constexpr float kInterpolationDelay = 0.20f;

    /// Maximum number of snapshots to retain in the buffer.  Older entries are
    /// dropped on push when the buffer exceeds this size, preventing unbounded
    /// memory growth if snapshots arrive faster than they are consumed.
    static constexpr std::size_t kMaxBufferSize = 20;

private:
    std::deque<Network::TransformSnapshot> buffer_;

    // Per-instance tuning (initialised from the static defaults; JSON can override)
    float       interpolationDelay_ = kInterpolationDelay;
    std::size_t maxBufferSize_      = kMaxBufferSize;

    /// Monotonically advancing render-playback clock (seconds).
    /// Starts at 0 and is incremented by deltaTime each frame.
    float renderTime_ = 0.0f;

    /// True once at least two snapshots have been received and the playback
    /// clock has been synchronised to the server timeline.
    bool  started_    = false;

    /// Position from the previous frame — used to compute currentSpeed_.
    glm::vec3 previousPosition_ = glm::vec3(0.0f);

    /// Most recently computed XZ movement speed (units/sec).
    float currentSpeed_ = 0.0f;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /// Remove stale snapshots that are no longer needed for interpolation.
    void pruneBuffer();

    /// Apply position + rotation from a single snapshot (used for hold/snap).
    void applySnapshot(const Network::TransformSnapshot& s);
};

#endif // ENGINE_NETWORKSYNCCOMPONENT_H

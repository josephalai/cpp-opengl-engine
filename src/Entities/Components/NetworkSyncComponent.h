// src/Entities/Components/NetworkSyncComponent.h
//
// Pure-data component that holds the state needed for remote-entity
// interpolation driven by server snapshots.
//
// Phase 2 Step 3 (complete): IComponent inheritance removed. This struct
// holds interpolation state only. All per-frame logic lives in
// NetworkSystem::update(), which reads and writes the public fields below.
//
// KEY CONCEPT — Entity Interpolation:
//   Remote entities do NOT snap directly to the newest snapshot.  Instead,
//   the component maintains a small playback clock that lags kInterpolationDelay
//   seconds behind the latest received snapshot.  Each frame the NetworkSystem
//   finds the two snapshots that bracket the current playback time and uses
//   glm::mix (LERP) for position and glm::slerp (quaternion) for rotation to
//   produce smooth, stutter-free movement even at a 10 Hz server tick rate.
//
// CLOCK SYNCHRONISATION:
//   On receiving the second snapshot the playback clock is seeded as:
//     renderTime_ = buffer_.back().timestamp
//   This places targetTime (= renderTime_ - kInterpolationDelay) exactly
//   kInterpolationDelay seconds behind the most-recent snapshot, giving a
//   steady-state forward buffer of kInterpolationDelay.  With
//   kInterpolationDelay = 2 × tick_interval (0.20 s) the client tolerates up
//   to ~100 ms of one-way network latency before the starvation branch fires.
//
// Edge cases handled (all logic in NetworkSystem::update()):
//   • Buffer empty        → entity is not moved (safe no-op).
//   • Only 1 snapshot     → entity is held at that position.
//   • Buffer starvation   → hold at the last known snapshot position.
//                           (Extrapolation was removed because it caused a
//                           visible "jump ahead + slingshot back" artefact
//                           whenever the remote entity changed speed/direction.)
//   • Normal interpolation→ smooth LERP/SLERP between bracketing snapshots.

#ifndef ENGINE_NETWORKSYNCCOMPONENT_H
#define ENGINE_NETWORKSYNCCOMPONENT_H

#include "../../Network/NetworkPackets.h"
#include <deque>
#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

struct NetworkSyncComponent {
    // -------------------------------------------------------------------------
    // JSON initialisation
    // -------------------------------------------------------------------------

    /// Load tuning parameters from a prefab JSON object.
    /// Supported keys:
    ///   "interpolation_delay" (float) — seconds of playback lag
    ///   "max_buffer_size"    (int)    — snapshot ring-buffer capacity
    void initFromJson(const nlohmann::json& j);

    // -------------------------------------------------------------------------
    // Server interface
    // -------------------------------------------------------------------------

    /// Push a new authoritative snapshot into the incoming buffer.
    void pushSnapshot(const Network::TransformSnapshot& snapshot);

    /// Returns the most recently computed XZ movement speed (units/sec).
    /// Computed each frame by NetworkSystem after position is applied; used by
    /// AnimationSystem to drive Walk/Run/Idle transitions for remote characters.
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

    // -------------------------------------------------------------------------
    // Pure data — read/written by NetworkSystem each frame
    // -------------------------------------------------------------------------
    std::deque<Network::TransformSnapshot> buffer_;

    float       interpolationDelay_ = kInterpolationDelay;
    std::size_t maxBufferSize_      = kMaxBufferSize;

    /// Monotonically advancing render-playback clock (seconds).
    /// Incremented by deltaTime each frame in NetworkSystem::update().
    float renderTime_ = 0.0f;

    /// True once at least two snapshots have been received and the playback
    /// clock has been synchronised to the server timeline.
    bool  started_    = false;

    /// Position from the previous frame — used to compute currentSpeed_.
    glm::vec3 previousPosition_ = glm::vec3(0.0f);

    /// True once previousPosition_ has been seeded from the entity's actual
    /// spawn position.  Prevents a bogus speed spike on the very first frame
    /// (when previousPosition_ would otherwise be the default-initialised origin).
    bool previousPositionInitialized_ = false;

    /// Most recently computed XZ movement speed (units/sec).
    float currentSpeed_ = 0.0f;
};

#endif // ENGINE_NETWORKSYNCCOMPONENT_H

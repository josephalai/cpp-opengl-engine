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
//   • Buffer starvation   → entity extrapolates from the last two snapshots
//                           (capped at 2× the inter-tick interval to avoid
//                           runaway extrapolation).
//   • Normal interpolation→ smooth LERP/SLERP between bracketing snapshots.

#ifndef ENGINE_NETWORKSYNCCOMPONENT_H
#define ENGINE_NETWORKSYNCCOMPONENT_H

#include "IComponent.h"
#include "../../Network/NetworkPackets.h"
#include <deque>

class NetworkSyncComponent : public IComponent {
public:
    // -------------------------------------------------------------------------
    // IComponent interface
    // -------------------------------------------------------------------------
    void init()   override {}
    void update(float deltaTime) override;

    // -------------------------------------------------------------------------
    // Server interface
    // -------------------------------------------------------------------------

    /// Push a new authoritative snapshot into the incoming buffer.
    /// Called by MockServer (or the real transport layer in Phase 2+) whenever a
    /// packet arrives.  Thread-safety: all calls must be from the main thread in
    /// Phase 1; Phase 2 will need a mutex here.
    void pushSnapshot(const Network::TransformSnapshot& snapshot);

    // -------------------------------------------------------------------------
    // Tuning constants
    // -------------------------------------------------------------------------

    /// How far behind the most-recent snapshot the render clock trails (seconds).
    /// A value of 1.5× the server tick interval guarantees we almost always have
    /// two bracketing snapshots available for smooth interpolation.
    static constexpr float kInterpolationDelay = 0.15f;

private:
    std::deque<Network::TransformSnapshot> buffer_;

    /// Monotonically advancing render-playback clock (seconds).
    /// Starts at 0 and is incremented by deltaTime each frame.
    float renderTime_ = 0.0f;

    /// True once at least two snapshots have been received and the playback
    /// clock has been synchronised to the server timeline.
    bool  started_    = false;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /// Remove stale snapshots that are no longer needed for interpolation.
    void pruneBuffer();

    /// Apply position + rotation from a single snapshot (used for hold/snap).
    void applySnapshot(const Network::TransformSnapshot& s);
};

#endif // ENGINE_NETWORKSYNCCOMPONENT_H

// src/Entities/Components/NetworkSyncComponent.cpp

#include "NetworkSyncComponent.h"
#include "../Entity.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// pushSnapshot
// ---------------------------------------------------------------------------

void NetworkSyncComponent::pushSnapshot(const Network::TransformSnapshot& snapshot) {
    // Discard out-of-order packets (sequence numbers must be monotonically
    // increasing; duplicates and late arrivals are silently dropped).
    if (!buffer_.empty() && snapshot.sequenceNumber <= buffer_.back().sequenceNumber) {
        return;
    }

    buffer_.push_back(snapshot);

    // Clamp the buffer to kMaxBufferSize to prevent memory bloat.
    while (buffer_.size() > kMaxBufferSize) {
        buffer_.pop_front();
    }

    // The first time we accumulate two snapshots we synchronise our playback
    // clock to the server timeline so renderTime_ - kInterpolationDelay gives
    // a targetTime that is slightly behind the earliest snapshot, allowing
    // smooth ramp-up from the very first frame.
    if (!started_ && buffer_.size() >= 2) {
        renderTime_ = buffer_.front().timestamp + kInterpolationDelay;
        started_    = true;
    }
}

// ---------------------------------------------------------------------------
// update — called every render frame
// ---------------------------------------------------------------------------

void NetworkSyncComponent::update(float deltaTime) {
    if (!entity_) return;

    // Nothing to do until we have received at least one snapshot.
    if (buffer_.empty()) return;

    // Advance the playback clock at real-time speed.
    renderTime_ += deltaTime;

    // The time we actually want to display (lagging kInterpolationDelay behind).
    const float targetTime = renderTime_ - kInterpolationDelay;

    // -----------------------------------------------------------------------
    // Hold case: only one snapshot or targetTime is before the earliest one.
    // -----------------------------------------------------------------------
    if (buffer_.size() == 1 || targetTime <= buffer_.front().timestamp) {
        applySnapshot(buffer_.front());

    // -----------------------------------------------------------------------
    // Starvation / extrapolation case: targetTime is beyond our newest snapshot.
    // -----------------------------------------------------------------------
    } else if (targetTime >= buffer_.back().timestamp) {
        if (buffer_.size() >= 2) {
            const auto& s0 = *(buffer_.end() - 2);
            const auto& s1 = buffer_.back();
            const float span = s1.timestamp - s0.timestamp;
            if (span > 0.0f) {
                // Velocity-based dead reckoning — cap over at ~1 tick to prevent
                // runaway drift while the next snapshot is in transit.
                const float over = std::min(targetTime - s1.timestamp, 0.1f);
                const glm::vec3 velocity = (s1.position - s0.position) / span;
                entity_->setPosition(s1.position + velocity * over);

                // Hold at the last known rotation during extrapolation to avoid
                // SLERP overshoot (t > 1) producing reversed orientations.
                entity_->setRotation(s1.rotation);
            } else {
                applySnapshot(s1);
            }
        } else {
            applySnapshot(buffer_.back());
        }
        pruneBuffer();

    // -----------------------------------------------------------------------
    // Normal case: find the two snapshots that bracket targetTime and LERP.
    // -----------------------------------------------------------------------
    } else {
        for (std::size_t i = 0; i + 1 < buffer_.size(); ++i) {
            const auto& s0 = buffer_[i];
            const auto& s1 = buffer_[i + 1];

            if (s0.timestamp <= targetTime && targetTime <= s1.timestamp) {
                const float span = s1.timestamp - s0.timestamp;
                const float t    = (span > 0.0f)
                                       ? glm::clamp((targetTime - s0.timestamp) / span, 0.0f, 1.0f)
                                       : 0.0f;

                // Interpolate position with LERP.
                entity_->setPosition(glm::mix(s0.position, s1.position, t));

                // Interpolate rotation with SLERP (quaternion) to avoid gimbal lock
                // and to produce the shortest angular arc.
                const glm::quat q0 = glm::quat(glm::radians(s0.rotation));
                const glm::quat q1 = glm::quat(glm::radians(s1.rotation));
                const glm::quat qi = glm::slerp(q0, q1, t);
                entity_->setRotation(glm::degrees(glm::eulerAngles(qi)));
                break;
            }
        }

        pruneBuffer();
    }

    // Compute XZ movement speed for animation-state decisions (Walk/Run/Idle).
    // This block executes for ALL non-empty-buffer cases (hold, starvation, and
    // normal interpolation) because the if-else chain above has no early returns.
    // previousPosition_ is therefore always kept up to date.
    {
        const glm::vec3 cur  = entity_->getPosition();
        const glm::vec3 d    = cur - previousPosition_;
        const float     dist = std::sqrt(d.x * d.x + d.z * d.z);
        currentSpeed_        = (deltaTime > 0.0f) ? dist / deltaTime : 0.0f;
        previousPosition_    = cur;
    }
}

// ---------------------------------------------------------------------------
// pruneBuffer
// ---------------------------------------------------------------------------

void NetworkSyncComponent::pruneBuffer() {
    // Retain every snapshot whose timestamp is >= (targetTime - one extra tick)
    // so we always keep two snapshots available for extrapolation.
    const float keepFrom = (renderTime_ - kInterpolationDelay) - kInterpolationDelay;
    while (buffer_.size() > 2 && buffer_.front().timestamp < keepFrom) {
        buffer_.pop_front();
    }
}

// ---------------------------------------------------------------------------
// applySnapshot
// ---------------------------------------------------------------------------

void NetworkSyncComponent::applySnapshot(const Network::TransformSnapshot& s) {
    entity_->setPosition(s.position);
    entity_->setRotation(s.rotation);
}

// src/Entities/Components/NetworkSyncComponent.cpp

#include "NetworkSyncComponent.h"
#include "../Entity.h"
#include <nlohmann/json.hpp>

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

    // Clamp the buffer to maxBufferSize_ to prevent memory bloat.
    while (buffer_.size() > maxBufferSize_) {
        buffer_.pop_front();
    }

    // The first time we accumulate two snapshots we synchronise our playback
    // clock to the server timeline.
    //
    // Setting renderTime_ = back.timestamp places targetTime exactly
    // interpolationDelay_ seconds BEHIND the most-recent received snapshot:
    //   targetTime = renderTime_ - interpolationDelay_
    //              = back.timestamp - interpolationDelay_
    //
    // This guarantees a steady-state gap of interpolationDelay_ between
    // targetTime and the latest received snapshot, so the client always has
    // ~interpolationDelay_ worth of future snapshots buffered before it needs
    // them.  With kInterpolationDelay = 0.20 s (2× the 0.10 s tick interval)
    // the client can tolerate up to ~100 ms of one-way network latency without
    // entering the starvation/extrapolation branch.
    //
    // Using buffer_.front().timestamp + interpolationDelay_ (the previous
    // formula) only provided a 1-tick forward window, meaning any positive
    // latency triggered extrapolation and the resulting jump-ahead / snap-back
    // artefact reported as NPC/remote entities moving too fast.
    if (!started_ && buffer_.size() >= 2) {
        renderTime_ = buffer_.back().timestamp;
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

    // --- CLOCK DRIFT CORRECTION ---
    // Prevent renderTime_ from running too far ahead of the server timeline.
    // If we've drifted, gently pull back to keep targetTime within the buffer.
    const float maxRenderTime = buffer_.back().timestamp + interpolationDelay_ + 0.01f;
    if (renderTime_ > maxRenderTime) {
        renderTime_ = maxRenderTime;
    }

    // The time we actually want to display (lagging interpolationDelay_ behind).
    const float targetTime = renderTime_ - interpolationDelay_;

    // -----------------------------------------------------------------------
    // Hold case: only one snapshot or targetTime is before the earliest one.
    // -----------------------------------------------------------------------
    if (buffer_.size() == 1 || targetTime <= buffer_.front().timestamp) {
        applySnapshot(buffer_.front());

    // -----------------------------------------------------------------------
    // Starvation case: targetTime is beyond our newest snapshot.
    // Hold at the last known position rather than extrapolating.
    //
    // Velocity-based dead reckoning was tried here, but it consistently
    // produces a "jump ahead + slingshot back" visual artefact whenever the
    // remote entity changes speed or direction between ticks.  The artefact
    // is more jarring than the mild "freeze" that a hold produces, so we
    // use the conservative hold strategy instead.
    // -----------------------------------------------------------------------
    } else if (targetTime >= buffer_.back().timestamp) {
        applySnapshot(buffer_.back());
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
    // On the very first update, seed previousPosition_ from the entity's actual
    // position so the first frame doesn't produce a bogus speed spike caused by
    // the default-initialised (0,0,0) sentinel.
    {
        const glm::vec3 cur = entity_->getPosition();
        if (!previousPositionInitialized_) {
            previousPosition_         = cur;
            previousPositionInitialized_ = true;
            currentSpeed_             = 0.0f;
        } else {
            const glm::vec3 d    = cur - previousPosition_;
            const float     dist = std::sqrt(d.x * d.x + d.z * d.z);
            currentSpeed_        = (deltaTime > 0.0f) ? dist / deltaTime : 0.0f;
        }
        previousPosition_ = cur;
    }
}

// ---------------------------------------------------------------------------
// pruneBuffer
// ---------------------------------------------------------------------------

void NetworkSyncComponent::pruneBuffer() {
    // Retain every snapshot whose timestamp is >= (targetTime - one extra tick)
    // so we always keep two snapshots available for extrapolation.
    const float keepFrom = (renderTime_ - interpolationDelay_) - interpolationDelay_;
    while (buffer_.size() > 2 && buffer_.front().timestamp < keepFrom) {
        buffer_.pop_front();
    }
}

// ---------------------------------------------------------------------------
// initFromJson — data-driven component initialisation (Phase 1, Step 3)
// ---------------------------------------------------------------------------

void NetworkSyncComponent::initFromJson(const nlohmann::json& j) {
    if (j.contains("interpolation_delay"))
        interpolationDelay_ = j["interpolation_delay"].get<float>();
    if (j.contains("max_buffer_size"))
        maxBufferSize_ = static_cast<std::size_t>(j["max_buffer_size"].get<int>());
}

// ---------------------------------------------------------------------------
// applySnapshot
// ---------------------------------------------------------------------------

void NetworkSyncComponent::applySnapshot(const Network::TransformSnapshot& s) {
    entity_->setPosition(s.position);
    entity_->setRotation(s.rotation);
}

// src/Engine/NetworkInterpolationSystem.cpp

#include "NetworkInterpolationSystem.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/AnimatedModelComponent.h"
#include "../ECS/Components/NetworkSyncData.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <iostream>

NetworkInterpolationSystem::NetworkInterpolationSystem(entt::registry& registry)
    : registry_(registry)
{}

void NetworkInterpolationSystem::update(float deltaTime) {
    auto view = registry_.view<TransformComponent, NetworkSyncData>();
    for (auto entity : view) {
        auto& tc  = view.get<TransformComponent>(entity);
        auto& nsd = view.get<NetworkSyncData>(entity);

        // Nothing to do until we have at least 2 snapshots to interpolate between.
        if (nsd.buffer.size() < 2) {
            if (!nsd.buffer.empty()) {
                tc.position = nsd.buffer.front().position;
                tc.rotation = nsd.buffer.front().rotation;
            }
            continue;
        }

        // =====================================================================
        // Simple two-snapshot interpolation with adaptive clock.
        //
        // We always interpolate between buffer[0] (s0) and buffer[1] (s1).
        // renderTime is a local interpolation timer that goes from 0.0 to 1.0
        // over the span between those two snapshots. When it reaches 1.0, we
        // pop s0 and start interpolating toward the next snapshot.
        //
        // The playback RATE adapts to how many snapshots are queued:
        //   - More queued  → play faster (catch up)
        //   - Fewer queued → play slower (avoid starvation)
        // This is the only speed control — no clamping, no fighting.
        // =====================================================================

        // Seed the interpolation timer on first use.
        if (!nsd.started) {
            nsd.renderTime = 0.0f;
            nsd.started = true;
        }

        const auto& s0 = nsd.buffer[0];
        const auto& s1 = nsd.buffer[1];
        const float span = s1.timestamp - s0.timestamp;

        // Adaptive playback rate based on queue depth.
        // Target: keep ~2-3 snapshots buffered (at 10 Hz that's 200-300ms).
        float playbackRate = 1.0f;
        size_t queued = nsd.buffer.size();
        if (queued > 5) {
            playbackRate = 1.5f;   // very behind — catch up aggressively
        } else if (queued > 3) {
            playbackRate = 1.2f;   // slightly behind
        } else if (queued <= 1) {
            playbackRate = 0.5f;   // starving — slow way down
        }

        // Advance the interpolation timer.
        // renderTime accumulates real seconds; t is the 0→1 fraction.
        if (span > 0.0001f) {
            nsd.renderTime += deltaTime * playbackRate;
        }

        float t = (span > 0.0001f) ? glm::clamp(nsd.renderTime / span, 0.0f, 1.0f) : 1.0f;

        // Interpolate position and rotation.
        tc.position = glm::mix(s0.position, s1.position, t);

        const glm::quat q0 = glm::quat(glm::radians(s0.rotation));
        const glm::quat q1 = glm::quat(glm::radians(s1.rotation));
        const glm::quat qi = glm::slerp(q0, q1, t);
        tc.rotation        = glm::degrees(glm::eulerAngles(qi));

        // Compute speed from the snapshot pair (stable for animation).
        if (span > 0.001f) {
            glm::vec3 diff = s1.position - s0.position;
            diff.y = 0.0f;
            nsd.currentSpeed = glm::length(diff) / span;
        } else {
            nsd.currentSpeed = 0.0f;
        }

        // When we've finished this segment, advance to the next pair.
        if (t >= 1.0f && nsd.buffer.size() > 2) {
            nsd.buffer.pop_front();
            nsd.renderTime = 0.0f;  // reset for the new s0→s1 segment
        }
        // If t >= 1.0 but only 2 snapshots left, just hold at s1's position
        // until a new snapshot arrives. No teleporting, no fighting.
    }
}
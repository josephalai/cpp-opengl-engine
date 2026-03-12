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

        // Nothing to do until we have received at least one snapshot.
        if (nsd.buffer.empty()) continue;

        if (!nsd.started) {
            // Not yet seeded — display the OLDEST snapshot so there is no
            // position jump when the clock seeds at buffer.front().timestamp +
            // interpolationDelay (see NetworkSystem.cpp).
            tc.position = nsd.buffer.front().position;
            tc.rotation = nsd.buffer.front().rotation;
            continue;
        }

        // --- NEW: ELASTIC CLOCK (RUBBER-BANDING) ---
        // Instead of chasing a jittery server packet timestamp, we scale our 
        // playback speed based on how full our queue of packets is.
        float timeScale = 1.0f;
        size_t bufferSize = nsd.buffer.size();

        if (bufferSize > 4) {
            // We have a backlog of packets. The client is falling behind the server.
            timeScale = 1.10f; // Play 10% faster to catch up
        } else if (bufferSize < 2) {
            // We are running out of packets. The client is advancing too fast.
            timeScale = 0.90f; // Play 10% slower to let the buffer refill
        }

        // Advance the playback clock using our elastic scale
        nsd.renderTime += (deltaTime * timeScale);

        // The time we actually want to display (lagging interpolationDelay behind).
        const float targetTime = nsd.renderTime - nsd.interpolationDelay;

        // -----------------------------------------------------------------------
        // Starvation case: targetTime is beyond our newest snapshot.
        // Snap/Hold at the newest position to avoid dangerous extrapolation overshoots.
        // -----------------------------------------------------------------------
        if (targetTime >= nsd.buffer.back().timestamp) {
            tc.position = nsd.buffer.back().position;
            tc.rotation = nsd.buffer.back().rotation;

        // -----------------------------------------------------------------------
        // Hold case: targetTime is before our earliest snapshot (massive lag spike recovery).
        // -----------------------------------------------------------------------
        } else if (targetTime <= nsd.buffer.front().timestamp) {
            tc.position = nsd.buffer.front().position;
            tc.rotation = nsd.buffer.front().rotation;

        // -----------------------------------------------------------------------
        // Normal case: find the two snapshots that bracket targetTime and LERP.
        // -----------------------------------------------------------------------
        } else {
            // Find the segment
            size_t interpIndex = 0;
            for (size_t i = 0; i + 1 < nsd.buffer.size(); ++i) {
                if (nsd.buffer[i].timestamp <= targetTime && targetTime <= nsd.buffer[i + 1].timestamp) {
                    interpIndex = i;
                    break;
                }
            }

            const auto& s0 = nsd.buffer[interpIndex];
            const auto& s1 = nsd.buffer[interpIndex + 1];

            const float span = s1.timestamp - s0.timestamp;
            const float t    = (span > 0.0f) ? glm::clamp((targetTime - s0.timestamp) / span, 0.0f, 1.0f) : 0.0f;

            // Interpolate position
            tc.position = glm::mix(s0.position, s1.position, t);

            // Interpolate rotation (SLERP)
            const glm::quat q0 = glm::quat(glm::radians(s0.rotation));
            const glm::quat q1 = glm::quat(glm::radians(s1.rotation));
            const glm::quat qi = glm::slerp(q0, q1, t);
            tc.rotation        = glm::degrees(glm::eulerAngles(qi));
            
            // --- NEW: SAFE BUFFER PRUNING ---
            // Discard packets that we have completely passed, BUT ALWAYS KEEP
            // the packet immediately preceding targetTime (buffer[0]) so we 
            // always have an `s0` to interpolate from!
            while (nsd.buffer.size() > 2 && nsd.buffer[1].timestamp < targetTime) {
                nsd.buffer.pop_front();
            }
        }

        // Compute XZ movement speed for animation-state decisions (Walk/Run/Idle).
        {
            const glm::vec3 cur = tc.position;
            if (!nsd.previousPositionInitialized) {
                nsd.previousPosition            = cur;
                nsd.previousPositionInitialized = true;
                nsd.currentSpeed                = 0.0f;
            } else {
                const glm::vec3 d        = cur - nsd.previousPosition;
                const float     dist     = std::sqrt(d.x * d.x + d.z * d.z);
                const float     rawSpeed = (deltaTime > 0.0f) ? dist / deltaTime : 0.0f;
                // Exponential moving average: smooths frame-to-frame noise so that
                // brief starvation frames (zero movement) don't immediately flip the
                // animation state to Idle.
                constexpr float kSpeedSmoothingAlpha = 0.15f;
                nsd.currentSpeed = glm::mix(nsd.currentSpeed, rawSpeed, kSpeedSmoothingAlpha);
            }
            nsd.previousPosition = cur;
        }
    }
}
// src/Engine/NetworkInterpolationSystem.cpp

#include "NetworkInterpolationSystem.h"
#include "../ECS/Components/TransformComponent.h"
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

        // In NetworkInterpolationSystem::update():
        // REMOVE the renderTime == 0.0f seed block entirely.
        // Let NetworkSystem.cpp be the SOLE clock seeder via the "started" flag.
        if (!nsd.started) {
            // Not yet seeded — just snap to latest position, don't advance clock
            tc.position = nsd.buffer.back().position;
            tc.rotation = nsd.buffer.back().rotation;
            continue;
        }

        // Advance the playback clock at real-time speed.
        nsd.renderTime += deltaTime;

        // After: nsd.renderTime += deltaTime;

        // The "ideal" renderTime is: latest server timestamp (i.e. what the
        // server clock reads RIGHT NOW, from our perspective).
        // We want renderTime to track (latestServerTime) so that
        // targetTime = renderTime - delay always has data to interpolate.
        float latestServerTime = nsd.buffer.back().timestamp;
        float idealRenderTime  = latestServerTime; // We want to be HERE

        // Gently steer renderTime toward the ideal.
        // This corrects both drift directions (too fast AND too slow).
        float clockError = idealRenderTime - nsd.renderTime;
        float correction = clockError * 0.1f; // 10% per frame = smooth convergence
        nsd.renderTime += correction;

        // --- CLOCK DRIFT CORRECTION ---
        const float maxRenderTime = nsd.buffer.back().timestamp + nsd.interpolationDelay + 0.01f;
        if (nsd.renderTime > maxRenderTime) {
            nsd.renderTime = maxRenderTime;
        }

        // The time we actually want to display (lagging interpolationDelay behind).
        const float targetTime = nsd.renderTime - nsd.interpolationDelay;

        // -----------------------------------------------------------------------
        // Hold case: only one snapshot or targetTime is before the earliest one.
        // -----------------------------------------------------------------------
        if (nsd.buffer.size() == 1 || targetTime <= nsd.buffer.front().timestamp) {
            tc.position = nsd.buffer.front().position;
            tc.rotation = nsd.buffer.front().rotation;

        // -----------------------------------------------------------------------
        // Starvation case: targetTime is beyond our newest snapshot.
        // Hold at the last known position rather than extrapolating.
        // -----------------------------------------------------------------------
        } else if (targetTime >= nsd.buffer.back().timestamp) {
            tc.position = nsd.buffer.back().position;
            tc.rotation = nsd.buffer.back().rotation;

            // Prune stale snapshots.
            const float keepFrom = (nsd.renderTime - nsd.interpolationDelay) - nsd.interpolationDelay;
            while (nsd.buffer.size() > 2 && nsd.buffer.front().timestamp < keepFrom) {
                nsd.buffer.pop_front();
            }

        // -----------------------------------------------------------------------
        // Normal case: find the two snapshots that bracket targetTime and LERP.
        // -----------------------------------------------------------------------
        } else {
            for (std::size_t i = 0; i + 1 < nsd.buffer.size(); ++i) {
                const auto& s0 = nsd.buffer[i];
                const auto& s1 = nsd.buffer[i + 1];

                if (s0.timestamp <= targetTime && targetTime <= s1.timestamp) {
                    const float span = s1.timestamp - s0.timestamp;
                    const float t    = (span > 0.0f)
                                           ? glm::clamp((targetTime - s0.timestamp) / span, 0.0f, 1.0f)
                                           : 0.0f;

                    // Interpolate position with LERP.
                    tc.position = glm::mix(s0.position, s1.position, t);

                    // Interpolate rotation with SLERP (quaternion) to avoid gimbal lock.
                    const glm::quat q0 = glm::quat(glm::radians(s0.rotation));
                    const glm::quat q1 = glm::quat(glm::radians(s1.rotation));
                    const glm::quat qi = glm::slerp(q0, q1, t);
                    tc.rotation        = glm::degrees(glm::eulerAngles(qi));
                    break;
                }
            }

            // Prune stale snapshots.
            const float keepFrom = (nsd.renderTime - nsd.interpolationDelay) - nsd.interpolationDelay;
            while (nsd.buffer.size() > 2 && nsd.buffer.front().timestamp < keepFrom) {
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
                const glm::vec3 d    = cur - nsd.previousPosition;
                const float     dist = std::sqrt(d.x * d.x + d.z * d.z);
                nsd.currentSpeed     = (deltaTime > 0.0f) ? dist / deltaTime : 0.0f;
            }
            nsd.previousPosition = cur;
        }
    }
}

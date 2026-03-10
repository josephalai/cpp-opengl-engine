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

        // --- NEW: CLOCK SEEDING ---
        // If this is the first time we are seeing this entity, sync its internal
        // render clock to the server's timestamp and enforce a 150ms playout delay.
        if (nsd.renderTime == 0.0f) {
            nsd.renderTime = nsd.buffer.front().timestamp;
            nsd.interpolationDelay = 0.25f; // 350ms buffer for a 10Hz server
        }

        // Advance the playback clock at real-time speed.
        nsd.renderTime += deltaTime;

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

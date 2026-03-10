// src/Engine/OriginShiftSystem.h
//
// Phase 4 Step 4.2.4 — Floating-point precision fix.
//
// Client side: When the camera exceeds a threshold distance from the local
// origin (default 4000 m), the system subtracts the camera offset from all
// local entities and terrain, resetting the player to (0,0,0) in render
// space.  This prevents the jitter that occurs with large float coordinates.
//
// Server side: The server stores absolute positions using the
// OriginShiftComponent (ChunkIndex + LocalFloat) for effectively infinite
// precision.  This header provides the client-side recentering logic.

#ifndef ENGINE_ORIGINSHIFTSYSTEM_H
#define ENGINE_ORIGINSHIFTSYSTEM_H

#include "ISystem.h"
#include "../ECS/Components/TransformComponent.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <cmath>

class OriginShiftSystem : public ISystem {
public:
    /// @param registry          ECS registry.
    /// @param shiftThreshold    Distance from origin before recentering (default 4000 m).
    OriginShiftSystem(entt::registry& registry, float shiftThreshold = 4000.0f)
        : registry_(registry), shiftThreshold_(shiftThreshold) {}

    void init() override {}

    /// Call each frame with the camera's world position.
    /// @return  The accumulated world-space offset that has been subtracted
    ///          from all entities.  Pass this to shaders / terrain to keep
    ///          everything aligned.
    glm::dvec3 update(const glm::vec3& cameraPos) {
        float dist = std::sqrt(cameraPos.x * cameraPos.x +
                               cameraPos.z * cameraPos.z);
        if (dist > shiftThreshold_) {
            // Accumulate the shift.
            glm::dvec3 shift(cameraPos.x, 0.0, cameraPos.z);
            accumulatedShift_ += shift;

            // Subtract the camera offset from ALL local entities.
            auto view = registry_.view<TransformComponent>();
            for (auto entity : view) {
                auto& tc = view.get<TransformComponent>(entity);
                tc.position.x -= cameraPos.x;
                tc.position.z -= cameraPos.z;
            }

            shiftCount_++;
        }
        return accumulatedShift_;
    }

    /// ISystem update() override — no-op; call the cameraPos overload.
    void update(float /*deltaTime*/) override {}

    /// Returns the total world-space offset applied.
    glm::dvec3 accumulatedShift() const { return accumulatedShift_; }

    /// Number of origin shifts performed so far.
    int shiftCount() const { return shiftCount_; }

    void shutdown() override {}

private:
    entt::registry& registry_;
    float           shiftThreshold_;
    glm::dvec3      accumulatedShift_ = {0.0, 0.0, 0.0};
    int             shiftCount_ = 0;
};

#endif // ENGINE_ORIGINSHIFTSYSTEM_H

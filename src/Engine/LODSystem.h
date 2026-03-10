// src/Engine/LODSystem.h
//
// Phase 4 Step 4.3.1 — Level of Detail system.
// Each frame, calculates the distance from the camera to each entity with
// a LODComponent and updates the currentLOD field.  The RenderSystem reads
// currentLOD to select the appropriate mesh variation.

#ifndef ENGINE_LODSYSTEM_H
#define ENGINE_LODSYSTEM_H

#include "ISystem.h"
#include "../ECS/Components/LODComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

class LODSystem : public ISystem {
public:
    LODSystem(entt::registry& registry) : registry_(registry) {}

    void init() override {}

    /// Update LOD levels for all entities with a LODComponent.
    /// @param cameraPos  World-space camera position used for distance calc.
    void update(const glm::vec3& cameraPos) {
        auto view = registry_.view<TransformComponent, LODComponent>();
        for (auto entity : view) {
            auto& tc  = view.get<TransformComponent>(entity);
            auto& lod = view.get<LODComponent>(entity);

            float dist = glm::length(cameraPos - tc.position);
            if (dist < lod.lodDistance0) {
                lod.currentLOD = 0;   // High poly
            } else if (dist < lod.lodDistance1) {
                lod.currentLOD = 1;   // Mid poly
            } else {
                lod.currentLOD = 2;   // Low poly
            }
        }
    }

    /// ISystem update() override (no-op; call the cameraPos overload instead).
    void update(float /*deltaTime*/) override {}

    void shutdown() override {}

private:
    entt::registry& registry_;
};

#endif // ENGINE_LODSYSTEM_H

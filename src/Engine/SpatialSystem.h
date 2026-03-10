// src/Engine/SpatialSystem.h
//
// Phase 4 Step 4.1.3 — Entity migration hook.
// Each tick, checks every entity with a SpatialComponent.  If the entity's
// world position maps to a different cell than the one stored in the
// component, the entity is migrated in the SpatialGrid and the component
// is updated.

#ifndef ENGINE_SPATIALSYSTEM_H
#define ENGINE_SPATIALSYSTEM_H

#include "ISystem.h"
#include "../Streaming/SpatialGrid.h"
#include "../ECS/Components/SpatialComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include <entt/entt.hpp>

class SpatialSystem : public ISystem {
public:
    /// @param registry  The ECS registry containing all entities.
    /// @param grid      The spatial grid used for AoI queries.
    SpatialSystem(entt::registry& registry, SpatialGrid& grid)
        : registry_(registry), grid_(grid) {}

    void init() override {}

    /// Check every entity with a SpatialComponent and migrate if its cell
    /// has changed since the last tick.
    void update(float /*deltaTime*/) override {
        auto view = registry_.view<TransformComponent, SpatialComponent>();
        for (auto entity : view) {
            auto& tc = view.get<TransformComponent>(entity);
            auto& sc = view.get<SpatialComponent>(entity);

            int newCX, newCZ;
            grid_.worldToCell(tc.position.x, tc.position.z, newCX, newCZ);

            if (newCX != sc.currentCellX || newCZ != sc.currentCellZ) {
                // Determine if the entity is static or dynamic.
                // Entities with a NetworkIdComponent are considered dynamic.
                bool isStatic = !registry_.any_of<NetworkIdComponent>(entity);

                grid_.migrateEntity(entity,
                                    sc.currentCellX, sc.currentCellZ,
                                    newCX, newCZ, isStatic);
                sc.currentCellX = newCX;
                sc.currentCellZ = newCZ;
            }
        }
    }

    /// Register a new entity in the spatial grid and attach a SpatialComponent.
    void registerEntity(entt::entity entity, const glm::vec3& pos, bool isStatic) {
        int cx, cz;
        grid_.worldToCell(pos.x, pos.z, cx, cz);

        if (!registry_.any_of<SpatialComponent>(entity)) {
            registry_.emplace<SpatialComponent>(entity, SpatialComponent{cx, cz});
        } else {
            auto& sc = registry_.get<SpatialComponent>(entity);
            sc.currentCellX = cx;
            sc.currentCellZ = cz;
        }
        grid_.addEntity(entity, cx, cz, isStatic);
    }

    /// Remove an entity from the spatial grid and its SpatialComponent.
    void unregisterEntity(entt::entity entity) {
        if (auto* sc = registry_.try_get<SpatialComponent>(entity)) {
            grid_.removeEntity(entity, sc->currentCellX, sc->currentCellZ);
        }
    }

    void shutdown() override {
        grid_.clear();
    }

private:
    entt::registry& registry_;
    SpatialGrid&    grid_;
};

#endif // ENGINE_SPATIALSYSTEM_H

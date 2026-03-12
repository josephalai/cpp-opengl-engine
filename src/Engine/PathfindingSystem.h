// src/Engine/PathfindingSystem.h
//
// Phase 4 Step 4.4.3 — Auto-steering via NavMesh waypoints.
// Entities with a PathfindingComponent are moved toward each waypoint in
// sequence.  When the player presses WASD (Step 4.4.4), the server removes
// the PathfindingComponent to return control to manual input.

#ifndef ENGINE_PATHFINDINGSYSTEM_H
#define ENGINE_PATHFINDINGSYSTEM_H

#include "ISystem.h"
#include "../ECS/Components/PathfindingComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/InputQueueComponent.h"
#include "../Physics/PhysicsSystem.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

class PathfindingSystem : public ISystem {
public:
    /// @param registry     The ECS registry.
    /// @param moveSpeed    Movement speed for auto-steered entities (m/s).
    /// @param physicsSystem Optional PhysicsSystem used to keep Bullet ghost
    ///                      positions in sync with the ECS TransformComponent so
    ///                      that the physics engine doesn't snap entities back to
    ///                      their pre-pathfinding positions each tick.
    PathfindingSystem(entt::registry& registry,
                      float           moveSpeed    = 20.0f,
                      PhysicsSystem*  physicsSystem = nullptr)
        : registry_(registry), moveSpeed_(moveSpeed), physicsSystem_(physicsSystem) {}

    void init() override {}

    void update(float deltaTime) override {
        // Collect entities to remove PathfindingComponent from (WASD interrupt).
        std::vector<entt::entity> toRemove;

        auto view = registry_.view<TransformComponent, PathfindingComponent>();
        for (auto entity : view) {
            auto& pc = view.get<PathfindingComponent>(entity);
            if (!pc.active || pc.waypoints.empty()) {
                toRemove.push_back(entity);
                continue;
            }

            // --- Step 4.4.4: WASD Interrupt ---
            // If the entity has pending manual input, cancel auto-steering.
            if (auto* iq = registry_.try_get<InputQueueComponent>(entity)) {
                if (!iq->inputs.empty()) {
                    // Check if any input has directional movement.
                    for (const auto& input : iq->inputs) {
                        if (input.moveForward || input.moveBackward ||
                            input.moveLeft    || input.moveRight) {
                            toRemove.push_back(entity);
                            break;
                        }
                    }
                    // If we're removing this entity from pathfinding, skip movement.
                    if (std::find(toRemove.begin(), toRemove.end(), entity) != toRemove.end())
                        continue;
                }
            }

            // --- Auto-steer toward the current waypoint ---
            auto& tc = view.get<TransformComponent>(entity);
            int idx = pc.currentWaypoint;
            if (idx >= static_cast<int>(pc.waypoints.size())) {
                toRemove.push_back(entity);
                continue;
            }

            glm::vec3 target = pc.waypoints[idx];
            glm::vec3 dir    = target - tc.position;
            float     dist2  = glm::dot(dir, dir);

            if (dist2 <= pc.arrivalRadius * pc.arrivalRadius) {
                // Waypoint reached — advance to next.
                pc.currentWaypoint++;
                if (pc.currentWaypoint >= static_cast<int>(pc.waypoints.size())) {
                    toRemove.push_back(entity);
                }
            } else {
                // Move toward the waypoint.
                dir = glm::normalize(dir);
                float step = moveSpeed_ * deltaTime;
                float dist = std::sqrt(dist2);
                if (step > dist) step = dist;

                // Drive movement through Bullet's character controller so that
                // PhysicsSystem::update() applies the displacement and the
                // ghost→tc sync produces the correct final position.  Direct
                // tc.position writes would be overwritten by the ghost sync the
                // following tick when no WASD input is present.
                if (physicsSystem_ && physicsSystem_->hasCharacterController(entity)) {
                    glm::vec3 displacement = dir * step;
                    physicsSystem_->setEntityWalkDirection(entity,
                        glm::vec3(displacement.x, 0.0f, displacement.z));
                } else {
                    // Fallback for entities without a character controller.
                    tc.position += dir * step;
                }

                // Face the direction of travel (yaw only).
                tc.rotation.y = glm::degrees(std::atan2(dir.x, dir.z));
            }
        }

        // Remove PathfindingComponent from entities that finished or were interrupted.
        for (auto entity : toRemove) {
            registry_.remove<PathfindingComponent>(entity);
        }
    }

    void shutdown() override {}

private:
    entt::registry& registry_;
    float           moveSpeed_;
    PhysicsSystem*  physicsSystem_ = nullptr;
};

#endif // ENGINE_PATHFINDINGSYSTEM_H

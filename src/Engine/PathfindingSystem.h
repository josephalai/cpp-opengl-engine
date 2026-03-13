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
        std::vector<entt::entity> toRemove;
        auto view = registry_.view<TransformComponent, PathfindingComponent>();

        for (auto entity : view) {
            auto& pc = view.get<PathfindingComponent>(entity);
            if (!pc.active || pc.waypoints.empty() || pc.currentWaypoint >= static_cast<int>(pc.waypoints.size())) {
                toRemove.push_back(entity);
                continue;
            }

            // --- WASD Interrupt ---
            if (auto* iq = registry_.try_get<InputQueueComponent>(entity)) {
                if (!iq->inputs.empty()) {
                    bool interrupted = false;
                    for (const auto& input : iq->inputs) {
                        if (input.moveForward || input.moveBackward || input.moveLeft || input.moveRight) {
                            toRemove.push_back(entity);
                            interrupted = true;
                            break;
                        }
                    }
                    if (interrupted) continue;
                }
            }

            auto& tc = view.get<TransformComponent>(entity);
            float remainingStep = moveSpeed_ * deltaTime;

            // --- NEW: CONSUME EXACT DISTANCE ACROSS MULTIPLE WAYPOINTS ---
            while (remainingStep > 0.0f && pc.currentWaypoint < static_cast<int>(pc.waypoints.size())) {
                glm::vec3 target = pc.waypoints[pc.currentWaypoint];
                glm::vec3 dir    = target - tc.position;
                dir.y = 0.0f; 
                
                float dist = glm::length(dir);

                if (dist <= pc.arrivalRadius) {
                    pc.currentWaypoint++;
                    continue; // Waypoint reached, use remainingStep on the next node
                }

                float step = std::min(remainingStep, dist);
                dir /= dist; // Normalize safely
                tc.position += dir * step;
                remainingStep -= step;

                // Face the direction of travel
                tc.rotation.y = glm::degrees(std::atan2(dir.x, dir.z));
            }

            // --- NEW: STRICT BULLET WARPING ---
            // Do not use setEntityWalkDirection! Warp the ghost directly to the exact NavMesh line.
            if (physicsSystem_ && physicsSystem_->hasCharacterController(entity)) {
                physicsSystem_->warpCharacterController(entity, tc.position);
            }

            if (pc.currentWaypoint >= static_cast<int>(pc.waypoints.size())) {
                toRemove.push_back(entity);
            }
        }

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

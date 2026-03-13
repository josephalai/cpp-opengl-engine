// src/Engine/AnimationSystem.cpp

#include "AnimationSystem.h"
#include "../RenderEngine/AnimatedRenderer.h"
#include "../Entities/Player.h"
#include "../ECS/Components/AnimatedModelComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkSyncData.h"
#include "../Input/InputMaster.h"
#include <iostream>

AnimationSystem::AnimationSystem(AnimatedRenderer*    renderer,
                                  entt::registry&      registry,
                                  Player*              player,
                                  std::vector<Light*>& lights,
                                  Camera*              camera,
                                  const glm::mat4&     projectionMatrix)
    : renderer_(renderer)
    , registry_(registry)
    , player_(player)
    , lights_(lights)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
{}

void AnimationSystem::update(float deltaTime) {
    auto view = registry_.view<AnimatedModelComponent, TransformComponent>();
    if (view.begin() == view.end()) return;

    // --- 1. Sync transforms and drive animation state machines ---
    for (auto entity : view) {
        auto& amc = view.get<AnimatedModelComponent>(entity);
        auto& tc  = view.get<TransformComponent>(entity);

                if (amc.isLocalPlayer && player_) {
            // Local player: follow physics-driven position.
            tc.position = player_->getPosition();
            tc.rotation = player_->getRotation();

            // --- Face direction of travel during auto-walk ---
            auto* isc = registry_.try_get<InputStateComponent>(entity);
            if (isc && isc->currentSpeed > 0.5f) {
                if (!InputMaster::isActionDown("MoveForward") &&
                    !InputMaster::isActionDown("MoveBackward")) {
                    glm::vec3 moveDir = tc.position - amc.lastPosition;
                    moveDir.y = 0.0f;
                    if (glm::dot(moveDir, moveDir) > 0.0001f) {
                        moveDir = glm::normalize(moveDir);
                        float yaw = glm::degrees(std::atan2(moveDir.x, moveDir.z));
                        amc.autoWalkYaw = yaw;
                        amc.useAutoWalkYaw = true;
                    }
                } else {
                    amc.useAutoWalkYaw = false;
                }
            } else {
                amc.useAutoWalkYaw = false;
            }
            amc.lastPosition = tc.position;

            // Apply the auto-walk yaw to the MODEL's transform only.
            // player_->getRotation() (which the camera reads) is NOT modified.
            if (amc.useAutoWalkYaw) {
                tc.rotation.y = amc.autoWalkYaw;
            }
        }
        // Remote entities: TransformComponent is already updated by
        // NetworkInterpolationSystem; no position overwrite needed here.

                // Drive animation state transitions based on network speed data.
        if (!amc.isLocalPlayer && amc.controller) {
            const auto* nsd = registry_.try_get<NetworkSyncData>(entity);
            if (nsd) {
                // Use actual per-frame position delta for speed instead of
                // snapshot-pair speed. This immediately reflects a stopped NPC.
                float speed = 0.0f;
                if (nsd->previousPositionInitialized && deltaTime > 0.0001f) {
                    glm::vec3 diff = tc.position - nsd->previousPosition;
                    diff.y = 0.0f;
                    speed = glm::length(diff) / deltaTime;
                }

                if (speed > 2.0f)
                    amc.controller->requestTransition("Run");
                else if (speed > 0.5f)
                    amc.controller->requestTransition("Walk");
                else if (speed < 0.3f)
                    amc.controller->requestTransition("Idle");
            }
        }
        // Update previous position for next-frame speed delta.
        if (!amc.isLocalPlayer) {
            auto* nsd = registry_.try_get<NetworkSyncData>(entity);
            if (nsd) {
                nsd->previousPosition = tc.position;
                nsd->previousPositionInitialized = true;
            }
        }
    }

    // --- 2. Model-offset tuning via Up/Down arrows ---
    // Hold Up to raise the mesh, Down to lower it. Offset is printed to stdout
    // at most 10 times/second for easy bake-in via scene.json offset= parameter.
    {
        static float offsetLogCooldown = 0.0f;
        offsetLogCooldown -= deltaTime;

        const float kOffsetSpeed = 0.5f;
        bool adjusted = false;

        if (InputMaster::isKeyDown(Up)) {
            for (auto entity : view) {
                auto& amc = view.get<AnimatedModelComponent>(entity);
                if (amc.isLocalPlayer) amc.modelOffset.y += kOffsetSpeed * deltaTime;
            }
            adjusted = true;
        } else if (InputMaster::isKeyDown(Down)) {
            for (auto entity : view) {
                auto& amc = view.get<AnimatedModelComponent>(entity);
                if (amc.isLocalPlayer) amc.modelOffset.y -= kOffsetSpeed * deltaTime;
            }
            adjusted = true;
        }

        if (adjusted && offsetLogCooldown <= 0.0f) {
            offsetLogCooldown = 0.1f;
            for (auto entity : view) {
                const auto& amc = view.get<AnimatedModelComponent>(entity);
                if (amc.isLocalPlayer) {
                    std::cout << "[ModelOffset] Y = " << amc.modelOffset.y << "\n";
                    break;
                }
            }
        }
    }

    // --- 3. Build temporary AnimatedEntity list for AnimatedRenderer ---
    // AnimatedRenderer still takes std::vector<AnimatedEntity*>.  We build a
    // temporary list each frame from the ECS data — no heap allocations per
    // entity since we store AnimatedEntity values in a local vector.
    std::vector<AnimatedEntity> tempStorage;
    tempStorage.reserve(16);
    std::vector<AnimatedEntity*> renderList;
    renderList.reserve(16);

    for (auto entity : view) {
        const auto& amc = view.get<AnimatedModelComponent>(entity);
        const auto& tc  = view.get<TransformComponent>(entity);
        if (!amc.model) continue;

        AnimatedEntity ae;
        ae.model        = amc.model;
        ae.controller   = amc.controller;
        ae.position     = tc.position;
        
        // APPLY YOUR NEW YAW HERE
        if (amc.isLocalPlayer && amc.useAutoWalkYaw) {
            ae.rotation = glm::vec3(tc.rotation.x, amc.autoWalkYaw, tc.rotation.z);
        } else {
            ae.rotation = tc.rotation;
        }

        ae.scale        = amc.scale;
        ae.modelOffset  = amc.modelOffset;
        ae.modelRotationMat = amc.modelRotationMat;
        ae.isLocalPlayer = amc.isLocalPlayer;
        ae.ownsModel    = false; 
        ae.pairedEntity = nullptr;
        tempStorage.push_back(ae);
        renderList.push_back(&tempStorage.back());
    }

    renderer_->render(renderList, deltaTime, lights_, camera_, projectionMatrix_);
}

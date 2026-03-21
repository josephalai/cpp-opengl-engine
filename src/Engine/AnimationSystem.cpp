// src/Engine/AnimationSystem.cpp

#include "AnimationSystem.h"
#include "../RenderEngine/AnimatedRenderer.h"
#include "../Entities/Player.h"
#include "../ECS/Components/AnimatedModelComponent.h"
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

            // On the very first frame the player position is valid, seed
            // lastPosition so the delta is zero rather than a huge jump from
            // (0,0,0) to the spawn point.  Skip all animation logic this frame.
            if (!amc.lastPositionInitialized) {
                amc.lastPosition            = tc.position;
                amc.lastPositionInitialized = true;
                continue;
            }

            // --- Compute per-frame movement delta (XZ only) ---
            glm::vec3 deltaPos = tc.position - amc.lastPosition;
            deltaPos.y = 0.0f;
            float deltaSq = glm::dot(deltaPos, deltaPos);

            bool anyKeyDown = InputMaster::isActionDown("MoveForward")  ||
                              InputMaster::isActionDown("MoveBackward") ||
                              InputMaster::isActionDown("MoveLeft")     ||
                              InputMaster::isActionDown("MoveRight");

            // If a server reconciliation warp just occurred and the player is
            // still holding a movement key, the position delta points in the
            // wrong direction (back toward where we were snapped from).  Reset
            // lastPosition so the next frame's delta is clean, restore the
            // input-driven yaw, and clear the flag.
            if (amc.wasSnappedBack && anyKeyDown) {
                amc.lastPosition   = tc.position;
                tc.rotation.y      = amc.lastInputYaw;
                amc.wasSnappedBack = false;
                // Recompute delta after the reset — it will be zero this frame.
                deltaPos = glm::vec3(0.0f);
                deltaSq  = 0.0f;
            } else if (amc.wasSnappedBack) {
                // No keys held; snap-back with no input — just clear the flag
                // so the delta-derived direction is used from next frame.
                amc.wasSnappedBack = false;
            }

            // While a server-authoritative LERP reconciliation is in progress,
            // treat the entity as moving regardless of key state.  This covers:
            //   • Keyboard + reconcile: anyKeyDown==true → use keyboard yaw.
            //   • Auto-walk reconcile: anyKeyDown==false → derive yaw from delta
            //     when a real delta exists, but NEVER flip isMoving to false due
            //     to a near-zero single-frame delta from warpPlayer().
            if (amc.suppressDeltaAnimation) {
                amc.isMoving     = true;
                amc.lastPosition = tc.position;
                if (anyKeyDown) {
                    amc.useAutoWalkYaw = false;  // keyboard yaw already correct
                    amc.lastInputYaw   = tc.rotation.y;
                } else if (deltaSq > 1e-4f) {
                    glm::vec3 dir = glm::normalize(deltaPos);
                    amc.autoWalkYaw    = glm::degrees(std::atan2(dir.x, dir.z));
                    amc.useAutoWalkYaw = true;
                }
                if (amc.controller) amc.controller->requestTransition("Walk");
                continue; // Skip remaining delta-based direction logic
            }

            // --- Bug 1 fix: keyboard movement ---
            // When any WASD key is held, PlayerMovementSystem already sets
            // tc.rotation.y = atan2(totalDx, totalDz) (facing direction).
            // Skip the delta-based yaw derivation entirely to avoid a one-frame
            // conflict between the two systems that caused visible micro-vibration.
            if (anyKeyDown) {
                amc.isMoving       = true;
                amc.useAutoWalkYaw = false;
                amc.lastInputYaw   = tc.rotation.y;
                amc.lastPosition   = tc.position;
                // Keep movingTimer positive so hysteresis holds after key release.
                amc.movingTimer = 0.15f;
                continue; // Trust PlayerMovementSystem's rotation — nothing else to do
            }

            // --- Auto-walk (click-to-walk, no keys held) ---
            // Bug 2 fix: use a hysteresis timer so a single near-zero delta frame
            // (e.g. from warpPlayer collision response) doesn't flip isMoving→false
            // and restart the Walk clip.
            if (deltaSq > 1e-4f) {
                amc.isMoving    = true;
                amc.movingTimer = 0.15f;  // Reset hold-open window while moving
                glm::vec3 dir = glm::normalize(deltaPos);
                amc.autoWalkYaw    = glm::degrees(std::atan2(dir.x, dir.z));
                amc.useAutoWalkYaw = true;
            } else {
                // No significant movement this frame — count down hysteresis timer.
                amc.movingTimer -= deltaTime;
                if (amc.movingTimer <= 0.0f) {
                    amc.movingTimer = 0.0f;
                    amc.isMoving    = false;
                }
                // If timer is still positive, isMoving stays true from the previous
                // frame — prevents single-frame idle transitions from restarting clips.
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
    // temporary list each frame from the ECS data.
    //
    // IMPORTANT: tempStorage must be fully populated BEFORE renderList is built.
    // Storing &tempStorage.back() while still push_back()-ing into tempStorage is
    // undefined behaviour: any push_back() that triggers a reallocation moves all
    // elements to a new address, invalidating every pointer already in renderList.
    // We fix this by reserving the exact entity count upfront, then building the
    // pointer list in a second pass once tempStorage is stable.
    std::vector<AnimatedEntity> tempStorage;
    {
        const auto count = static_cast<size_t>(view.size_hint());
        tempStorage.reserve(count ? count : 16);
    }

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
    }

    // Build the pointer list in a second pass now that tempStorage is stable
    // (no further push_back()s, so no reallocation can invalidate the pointers).
    std::vector<AnimatedEntity*> renderList;
    renderList.reserve(tempStorage.size());
    for (auto& ae : tempStorage)
        renderList.push_back(&ae);

    renderer_->render(renderList, deltaTime, lights_, camera_, projectionMatrix_);
}

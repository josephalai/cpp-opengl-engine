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
        }
        // Remote entities: TransformComponent is already updated by
        // NetworkInterpolationSystem; no position overwrite needed here.

        // Drive animation state transitions based on network speed data.
        if (!amc.isLocalPlayer && amc.controller) {
            const auto* nsd = registry_.try_get<NetworkSyncData>(entity);
            if (nsd) {
                const float speed = nsd->currentSpeed;
                // Use raised thresholds with a hysteresis dead-zone to prevent
                // rapid Walk↔Idle toggling caused by frame-to-frame speed noise.
                // Idle is only entered below 0.3; Walk only starts above 0.5.
                // The band [0.3, 0.5] keeps the current state unchanged.
                if (speed > 2.0f)
                    amc.controller->requestTransition("Run");
                else if (speed > 0.5f)
                    amc.controller->requestTransition("Walk");
                else if (speed < 0.3f)
                    amc.controller->requestTransition("Idle");
                // else: within dead-zone [0.3, 0.5] — keep current state.
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
        ae.rotation     = tc.rotation;
        ae.scale        = amc.scale;
        ae.modelOffset  = amc.modelOffset;
        ae.modelRotationMat = amc.modelRotationMat;
        ae.isLocalPlayer = amc.isLocalPlayer;
        ae.ownsModel    = false;  // ownership stays in the component
        ae.pairedEntity = nullptr;
        tempStorage.push_back(ae);
        renderList.push_back(&tempStorage.back());
    }

    renderer_->render(renderList, deltaTime, lights_, camera_, projectionMatrix_);
}

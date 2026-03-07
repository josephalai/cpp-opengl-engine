// src/Engine/AnimationSystem.cpp
//
// Phase 2 Step 3 — Pure Systems.
//   Iterates registry.view<AnimatedEntity>() to sync + render animated characters.
//   Lights are discovered via registry.view<LightComponent>() each frame.

#include "AnimationSystem.h"
#include "../RenderEngine/AnimatedRenderer.h"
#include "../Entities/Player.h"
#include "../Entities/Entity.h"
#include "../Entities/Components/NetworkSyncComponent.h"
#include "../ECS/Components/LightComponent.h"
#include "../Entities/Light.h"
#include "../Input/InputMaster.h"
#include "../RenderEngine/DisplayManager.h"
#include <iostream>

AnimationSystem::AnimationSystem(AnimatedRenderer*  renderer,
                                  entt::registry&    registry,
                                  Player*            player,
                                  Camera*            camera,
                                  const glm::mat4&   projectionMatrix)
    : renderer_(renderer)
    , registry_(registry)
    , player_(player)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
{}

void AnimationSystem::update(float deltaTime) {
    auto animView = registry_.view<AnimatedEntity>();
    if (animView.begin() == animView.end()) return;

    // Sync local player animated entity transform to the physics-driven player.
    if (player_) {
        for (auto [e, ae] : animView.each()) {
            if (!ae.isLocalPlayer) continue;
            ae.position = player_->getPosition();
            ae.rotation = player_->getRotation();
        }
    }

    // Sync remote animated entities from their paired Entity (NetworkSyncComponent)
    // and drive animation state transitions based on computed movement speed.
    for (auto [e, ae] : animView.each()) {
        if (ae.isLocalPlayer) continue;
        if (ae.pairedEntity) {
            ae.position = ae.pairedEntity->getPosition();
            ae.rotation = ae.pairedEntity->getRotation();
        }
        if (ae.controller && ae.pairedEntity) {
            auto* sync = ae.pairedEntity->getComponent<NetworkSyncComponent>();
            if (sync) {
                const float speed = sync->getCurrentSpeed();
                if (speed > 1.0f) {
                    ae.controller->requestTransition("Run");
                } else if (speed > 0.1f) {
                    ae.controller->requestTransition("Walk");
                } else {
                    ae.controller->requestTransition("Idle");
                }
            }
        }
    }

    // Model-offset tuning via Up/Down arrows (adjust visual mesh Y offset).
    {
        static float offsetLogCooldown = 0.0f;
        offsetLogCooldown -= deltaTime;

        const float kOffsetSpeed = 0.5f;
        bool adjusted = false;

        if (InputMaster::isKeyDown(Up)) {
            for (auto [e, ae] : animView.each())
                if (ae.isLocalPlayer) ae.modelOffset.y += kOffsetSpeed * deltaTime;
            adjusted = true;
        } else if (InputMaster::isKeyDown(Down)) {
            for (auto [e, ae] : animView.each())
                if (ae.isLocalPlayer) ae.modelOffset.y -= kOffsetSpeed * deltaTime;
            adjusted = true;
        }

        if (adjusted && offsetLogCooldown <= 0.0f) {
            offsetLogCooldown = 0.1f;
            for (auto [e, ae] : animView.each()) {
                if (ae.isLocalPlayer) {
                    std::cout << "[ModelOffset] Y = " << ae.modelOffset.y << "\n";
                    break;
                }
            }
        }
    }

    // Collect AnimatedEntity* pointers (stable within this call) for the renderer.
    std::vector<AnimatedEntity*> ptrs;
    for (auto [e, ae] : animView.each()) {
        ptrs.push_back(&ae);
    }

    // Build lights list from registry.
    std::vector<Light*> lights;
    {
        auto lightView = registry_.view<LightComponent>();
        for (auto [e, lc] : lightView.each()) {
            if (lc.light) lights.push_back(lc.light);
        }
    }

    renderer_->render(ptrs, deltaTime, lights, camera_, projectionMatrix_);
}

// src/Engine/AnimationSystem.cpp

#include "AnimationSystem.h"
#include "../RenderEngine/AnimatedRenderer.h"
#include "../Entities/Player.h"
#include "../Entities/Entity.h"
#include "../Entities/Components/NetworkSyncComponent.h"
#include "../Input/InputMaster.h"
#include "../RenderEngine/DisplayManager.h"
#include <iostream>

AnimationSystem::AnimationSystem(AnimatedRenderer*             renderer,
                                  std::vector<AnimatedEntity*>& entities,
                                  Player*                       player,
                                  std::vector<Light*>&          lights,
                                  Camera*                       camera,
                                  const glm::mat4&              projectionMatrix)
    : renderer_(renderer)
    , entities_(entities)
    , player_(player)
    , lights_(lights)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
{}

void AnimationSystem::update(float deltaTime) {
    if (entities_.empty()) return;

    // Sync local player animated entity transform to the physics-driven player.
    if (player_) {
        for (auto* ae : entities_) {
            if (!ae || !ae->isLocalPlayer) continue;
            ae->position = player_->getPosition();
            ae->rotation = player_->getRotation();
        }
    }

    // Sync remote animated entities from their paired Entity (NetworkSyncComponent)
    // and drive animation state transitions based on computed movement speed.
    for (auto* ae : entities_) {
        if (!ae || ae->isLocalPlayer) continue;
        if (ae->pairedEntity) {
            ae->position = ae->pairedEntity->getPosition();
            ae->rotation = ae->pairedEntity->getRotation();
        }
        if (ae->controller && ae->pairedEntity) {
            auto* sync = ae->pairedEntity->getComponent<NetworkSyncComponent>();
            if (sync) {
                const float speed = sync->getCurrentSpeed();
                if (speed > 1.0f) {
                    ae->controller->requestTransition("Run");
                } else if (speed > 0.1f) {
                    ae->controller->requestTransition("Walk");
                } else {
                    ae->controller->requestTransition("Idle");
                }
            }
        }
    }

    // Model-offset tuning via Up/Down arrows — nudges the visual mesh along Y
    // without touching the physics body, so you can align skin to capsule.
    // Hold Up to raise the mesh, Down to lower it. Offset is printed to stdout
    // at most 10 times/second for easy bake-in via scene.cfg offset= parameter.
    {
        static float offsetLogCooldown = 0.0f;
        offsetLogCooldown -= deltaTime;

        const float kOffsetSpeed = 0.5f;
        bool adjusted = false;

        if (InputMaster::isKeyDown(Up)) {
            for (auto* ae : entities_)
                if (ae && ae->isLocalPlayer) ae->modelOffset.y += kOffsetSpeed * deltaTime;
            adjusted = true;
        } else if (InputMaster::isKeyDown(Down)) {
            for (auto* ae : entities_)
                if (ae && ae->isLocalPlayer) ae->modelOffset.y -= kOffsetSpeed * deltaTime;
            adjusted = true;
        }

        if (adjusted && offsetLogCooldown <= 0.0f) {
            offsetLogCooldown = 0.1f;
            // Print the offset of the first local-player entity for bake-in.
            for (auto* ae : entities_) {
                if (ae && ae->isLocalPlayer) {
                    std::cout << "[ModelOffset] Y = " << ae->modelOffset.y << "\n";
                    break;
                }
            }
        }
    }

    renderer_->render(entities_, deltaTime, lights_, camera_, projectionMatrix_);
}

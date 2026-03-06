// src/Engine/AnimationSystem.cpp

#include "AnimationSystem.h"
#include "../RenderEngine/AnimatedRenderer.h"
#include "../Entities/Player.h"
#include "../Entities/Entity.h"
#include "../Input/InputMaster.h"
#include "../RenderEngine/DisplayManager.h"
#include <iostream>

AnimationSystem::AnimationSystem(AnimatedRenderer*                                    renderer,
                                  std::vector<AnimatedEntity*>&                        entities,
                                  Player*                                              player,
                                  std::vector<Light*>&                                 lights,
                                  Camera*                                              camera,
                                  const glm::mat4&                                     projectionMatrix,
                                  const std::unordered_map<Entity*, AnimatedEntity*>&  remoteAnimMap)
    : renderer_(renderer)
    , entities_(entities)
    , player_(player)
    , lights_(lights)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
    , remoteAnimMap_(remoteAnimMap)
{}

void AnimationSystem::update(float deltaTime) {
    if (entities_.empty()) return;

    // Sync remote animated entities from their paired physics Entity's current
    // (NetworkSyncComponent-interpolated) position and rotation.
    for (auto& [ent, ae] : remoteAnimMap_) {
        if (!ent || !ae) continue;
        ae->position = ent->getPosition();
        ae->rotation = ent->getRotation();
    }

    // Sync only LOCAL animated entities' world transform to the player.
    // Remote entities (isLocal == false) are driven by the loop above.
    if (player_) {
        for (auto* ae : entities_) {
            if (!ae || !ae->isLocal) continue;
            ae->position = player_->getPosition();
            ae->rotation = player_->getRotation();
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
                if (ae) ae->modelOffset.y += kOffsetSpeed * deltaTime;
            adjusted = true;
        } else if (InputMaster::isKeyDown(Down)) {
            for (auto* ae : entities_)
                if (ae) ae->modelOffset.y -= kOffsetSpeed * deltaTime;
            adjusted = true;
        }

        if (adjusted && offsetLogCooldown <= 0.0f) {
            offsetLogCooldown = 0.1f;
            if (entities_[0]) {
                std::cout << "[ModelOffset] Y = " << entities_[0]->modelOffset.y;
                if (entities_.size() > 1)
                    std::cout << "  (applied to all " << entities_.size() << " entities)";
                std::cout << "\n";
            }
        }
    }

    renderer_->render(entities_, deltaTime, lights_, camera_, projectionMatrix_);
}


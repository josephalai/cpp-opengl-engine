// src/Engine/AnimationSystem.cpp

#include "AnimationSystem.h"
#include "../RenderEngine/AnimatedRenderer.h"
#include "../Entities/Player.h"

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

    // Sync every animated character's world transform to the player
    if (player_) {
        for (auto* ae : entities_) {
            if (!ae) continue;
            ae->position = player_->getPosition();
            ae->rotation = player_->getRotation();
        }
    }

    renderer_->render(entities_, deltaTime, lights_, camera_, projectionMatrix_);
}

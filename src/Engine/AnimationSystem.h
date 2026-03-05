// src/Engine/AnimationSystem.h
// Subsystem that advances animation controllers and renders animated entities.
// Syncs animated entity transforms to the player each frame and delegates
// to AnimatedRenderer for skinned-mesh rendering.

#ifndef ENGINE_ANIMATIONSYSTEM_H
#define ENGINE_ANIMATIONSYSTEM_H

#include "ISystem.h"
#include <vector>
#include <glm/glm.hpp>

class AnimatedRenderer;
struct AnimatedEntity;
class Player;
class Camera;
class Light;

class AnimationSystem : public ISystem {
public:
    AnimationSystem(AnimatedRenderer*             renderer,
                    std::vector<AnimatedEntity*>& entities,
                    Player*                       player,
                    std::vector<Light*>&          lights,
                    Camera*                       camera,
                    const glm::mat4&              projectionMatrix);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    AnimatedRenderer*             renderer_;
    std::vector<AnimatedEntity*>& entities_;
    Player*                       player_;
    std::vector<Light*>&          lights_;
    Camera*                       camera_;
    glm::mat4                     projectionMatrix_;
};

#endif // ENGINE_ANIMATIONSYSTEM_H

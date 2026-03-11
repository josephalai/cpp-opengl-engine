// src/Engine/AnimationSystem.h
// Subsystem that advances animation controllers and renders animated entities.
// Drives all AnimatedModelComponent ECS entities — local player and remote alike.

#ifndef ENGINE_ANIMATIONSYSTEM_H
#define ENGINE_ANIMATIONSYSTEM_H

#include "ISystem.h"
#include <vector>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

class AnimatedRenderer;
class Player;
class Camera;
class Light;

class AnimationSystem : public ISystem {
public:
    AnimationSystem(AnimatedRenderer*    renderer,
                    entt::registry&      registry,
                    Player*              player,
                    std::vector<Light*>& lights,
                    Camera*              camera,
                    const glm::mat4&     projectionMatrix);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    AnimatedRenderer*    renderer_;
    entt::registry&      registry_;
    Player*              player_;
    std::vector<Light*>& lights_;
    Camera*              camera_;
    glm::mat4            projectionMatrix_;
};

#endif // ENGINE_ANIMATIONSYSTEM_H

// src/Engine/AnimationSystem.h
// Subsystem that advances animation controllers and renders animated entities.
// Syncs animated entity transforms to the player (local) or paired Entity
// (remote via NetworkSyncComponent) each frame and delegates to AnimatedRenderer.
//
// Phase 2 Step 3 — Pure Systems:
//   Iterates registry.view<AnimatedEntity>() instead of a legacy side-vector.
//   Lights are discovered via registry.view<LightComponent>() each frame.

#ifndef ENGINE_ANIMATIONSYSTEM_H
#define ENGINE_ANIMATIONSYSTEM_H

#include "ISystem.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

class AnimatedRenderer;
class Player;
class Camera;

class AnimationSystem : public ISystem {
public:
    AnimationSystem(AnimatedRenderer*  renderer,
                    entt::registry&    registry,
                    Player*            player,
                    Camera*            camera,
                    const glm::mat4&   projectionMatrix);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    AnimatedRenderer*  renderer_;
    entt::registry&    registry_;
    Player*            player_;
    Camera*            camera_;
    glm::mat4          projectionMatrix_;
};

#endif // ENGINE_ANIMATIONSYSTEM_H

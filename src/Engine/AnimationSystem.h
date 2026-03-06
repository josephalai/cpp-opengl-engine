// src/Engine/AnimationSystem.h
// Subsystem that advances animation controllers and renders animated entities.
// Syncs local animated entity transform to the player each frame, syncs remote
// animated entities from their paired physics Entity, and delegates to
// AnimatedRenderer for skinned-mesh rendering.

#ifndef ENGINE_ANIMATIONSYSTEM_H
#define ENGINE_ANIMATIONSYSTEM_H

#include "ISystem.h"
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

class AnimatedRenderer;
struct AnimatedEntity;
class Entity;
class Player;
class Camera;
class Light;

class AnimationSystem : public ISystem {
public:
    /// @param remoteAnimMap  Maps physics Entity* → AnimatedEntity* for each
    ///                       remote player.  The AnimationSystem reads the
    ///                       Entity's current (interpolated) position each frame
    ///                       and writes it to the paired AnimatedEntity so the
    ///                       skinned mesh follows the network-driven capsule.
    AnimationSystem(AnimatedRenderer*                                    renderer,
                    std::vector<AnimatedEntity*>&                        entities,
                    Player*                                              player,
                    std::vector<Light*>&                                 lights,
                    Camera*                                              camera,
                    const glm::mat4&                                     projectionMatrix,
                    const std::unordered_map<Entity*, AnimatedEntity*>&  remoteAnimMap);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    AnimatedRenderer*                                    renderer_;
    std::vector<AnimatedEntity*>&                        entities_;
    Player*                                              player_;
    std::vector<Light*>&                                 lights_;
    Camera*                                              camera_;
    glm::mat4                                            projectionMatrix_;
    const std::unordered_map<Entity*, AnimatedEntity*>&  remoteAnimMap_;
};

#endif // ENGINE_ANIMATIONSYSTEM_H

// src/Animation/AnimationBlender.h
// Blends between two AnimationClip outputs with configurable blend factor.

#ifndef ENGINE_ANIMATIONBLENDER_H
#define ENGINE_ANIMATIONBLENDER_H

#include <vector>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include "Skeleton.h"
#include "AnimationClip.h"

class AnimationBlender {
public:
    /// Blend two clips at their respective times into the skeleton.
    /// blendFactor: 0.0 = 100% clipA, 1.0 = 100% clipB.
    static void blend(const AnimationClip& clipA, float timeA,
                      const AnimationClip& clipB, float timeB,
                      float blendFactor, Skeleton& skeleton);

    /// Compute a crossfade blend factor that ramps from 0→1 over blendDuration.
    /// elapsed is how many seconds have passed since the transition started.
    static float crossfadeFactor(float elapsed, float blendDuration);
};

#endif // ENGINE_ANIMATIONBLENDER_H

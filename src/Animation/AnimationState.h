// src/Animation/AnimationState.h
// A single named animation state (e.g. "Idle", "Walk", "Run", "Jump").

#ifndef ENGINE_ANIMATIONSTATE_H
#define ENGINE_ANIMATIONSTATE_H

#include <string>
#include "AnimationClip.h"
#include "Skeleton.h"

class AnimationState {
public:
    std::string     name;
    AnimationClip*  clip       = nullptr;  ///< Non-owning pointer.
    float           speed      = 1.0f;     ///< Playback multiplier.
    bool            looping    = true;
    float           currentTime = 0.0f;   ///< Current position in seconds.

    AnimationState() = default;
    AnimationState(const std::string& n, AnimationClip* c, float spd = 1.0f, bool loop = true)
        : name(n), clip(c), speed(spd), looping(loop), currentTime(0.0f) {}

    /// Advance by deltaTime and apply the clip to the skeleton.
    /// Returns true while still playing (always true when looping).
    bool update(float deltaTime, Skeleton& skeleton);

    void reset() { currentTime = 0.0f; }
};

#endif // ENGINE_ANIMATIONSTATE_H

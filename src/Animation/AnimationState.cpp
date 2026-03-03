// src/Animation/AnimationState.cpp

#include "AnimationState.h"
#include <cmath>

bool AnimationState::update(float deltaTime, Skeleton& skeleton) {
    if (!clip) return false;

    currentTime += deltaTime * speed;

    float duration = clip->getDurationSeconds();
    if (duration <= 0.0f) duration = 1.0f;

    if (looping) {
        currentTime = std::fmod(currentTime, duration);
        if (currentTime < 0.0f) currentTime += duration;
    } else {
        if (currentTime >= duration) {
            currentTime = duration;
            clip->interpolate(currentTime, skeleton);
            return false; // finished
        }
    }

    clip->interpolate(currentTime, skeleton);
    return true;
}

// src/Animation/AnimationController.cpp

#include "AnimationController.h"
#include <cmath>
#include <iostream>

void AnimationController::addState(const std::string& name, AnimationClip* clip,
                                    float speed, bool looping) {
    states[name] = AnimationState(name, clip, speed, looping);
}

void AnimationController::addTransition(const std::string& from, const std::string& to,
                                         std::function<bool()> condition, float bDuration) {
    transitions.push_back({ from, to, condition, bDuration });
}

void AnimationController::setState(const std::string& name) {
    if (states.find(name) == states.end()) {
        std::cerr << "[AnimationController] Unknown state: " << name << "\n";
        return;
    }
    if (name == currentStateName) return;

    previousStateName = currentStateName;
    currentStateName  = name;
    states[name].reset();

    // Determine blend duration from the matching transition
    blendDuration = 0.3f;
    for (const auto& t : transitions) {
        if (t.from == previousStateName && t.to == name) {
            blendDuration = t.blendDuration;
            break;
        }
    }

    blending     = !previousStateName.empty();
    blendElapsed = 0.0f;
}

std::vector<glm::mat4> AnimationController::update(float deltaTime, Skeleton& skeleton) {
    if (currentStateName.empty()) {
        return skeleton.computeBoneMatrices();
    }

    // Check automatic transitions
    for (const auto& t : transitions) {
        if (t.from == currentStateName && t.condition && t.condition()) {
            setState(t.to);
            break;
        }
    }

    if (blending && !previousStateName.empty()) {
        blendElapsed += deltaTime;
        float factor = AnimationBlender::crossfadeFactor(blendElapsed, blendDuration);

        AnimationState& prev = states[previousStateName];
        AnimationState& curr = states[currentStateName];

        // Advance both times
        prev.currentTime += deltaTime * prev.speed;
        curr.currentTime += deltaTime * curr.speed;

        float prevDur = prev.clip ? prev.clip->getDurationSeconds() : 1.0f;
        float currDur = curr.clip ? curr.clip->getDurationSeconds() : 1.0f;
        if (prev.looping && prevDur > 0.0f)
            prev.currentTime = std::fmod(prev.currentTime, prevDur);
        if (curr.looping && currDur > 0.0f)
            curr.currentTime = std::fmod(curr.currentTime, currDur);

        if (prev.clip && curr.clip) {
            AnimationBlender::blend(*prev.clip, prev.currentTime,
                                    *curr.clip, curr.currentTime,
                                    factor, skeleton);
        }

        if (factor >= 1.0f) blending = false;
    } else {
        // Single state playback
        states[currentStateName].update(deltaTime, skeleton);
    }

    return skeleton.computeBoneMatrices();
}

void AnimationController::setupDefaultTransitions(std::function<bool()> walkCond,
                                                   std::function<bool()> runCond,
                                                   std::function<bool()> jumpCond) {
    // Inverse conditions: transition back when the triggering input is released
    auto notWalkNotRun = [walkCond, runCond]() { return !walkCond() && !runCond(); };
    auto walkOnly      = [walkCond]()          { return walkCond(); };

    addTransition("Idle", "Walk", walkCond,      0.2f);
    addTransition("Walk", "Idle", notWalkNotRun, 0.2f);
    addTransition("Walk", "Run",  runCond,       0.15f);
    addTransition("Run",  "Walk", walkOnly,      0.15f);
    addTransition("Idle", "Jump", jumpCond,      0.1f);
    addTransition("Walk", "Jump", jumpCond,      0.1f);
    addTransition("Run",  "Jump", jumpCond,      0.1f);
    addTransition("Jump", "Idle", nullptr,       0.2f);
}

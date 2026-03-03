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
    // Empty name → return to bind-pose (no active clip).
    if (name.empty()) {
        previousStateName = currentStateName;
        currentStateName  = "";
        blending          = false;   // no crossfade when dropping to bind pose
        blendElapsed      = 0.0f;
        return;
    }

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
    // Check automatic transitions first — this handles the bind-pose ("") → movement
    // case as well as the normal state-to-state transitions.
    for (const auto& t : transitions) {
        if (t.from == currentStateName && t.condition && t.condition()) {
            setState(t.to);
            break;
        }
    }

    if (currentStateName.empty()) {
        // No active clip — return bind-pose matrices.
        return skeleton.computeBoneMatrices();
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

    bool hasIdle = states.count("Idle") > 0;
    bool hasWalk = states.count("Walk") > 0;
    bool hasRun  = states.count("Run")  > 0;
    bool hasJump = states.count("Jump") > 0;

    // Standard transitions: only add when both endpoint states exist.
    if (hasIdle && hasWalk) {
        addTransition("Idle", "Walk", walkCond,      0.2f);
        addTransition("Walk", "Idle", notWalkNotRun, 0.2f);
    }
    if (hasWalk && hasRun) {
        addTransition("Walk", "Run",  runCond,  0.15f);
        addTransition("Run",  "Walk", walkOnly, 0.15f);
    }
    if (hasIdle && hasJump) addTransition("Idle", "Jump", jumpCond, 0.1f);
    if (hasWalk && hasJump) addTransition("Walk", "Jump", jumpCond, 0.1f);
    if (hasRun  && hasJump) addTransition("Run",  "Jump", jumpCond, 0.1f);
    if (hasJump && hasIdle) addTransition("Jump", "Idle", nullptr,  0.2f);

    // For models with no Idle clip: add bind-pose ("") ↔ movement transitions so
    // the character stands still in T-pose until the player provides input.
    if (!hasIdle) {
        if (hasWalk) {
            addTransition("",     "Walk", walkCond,      0.0f);
            addTransition("Walk", "",     notWalkNotRun, 0.0f);
        }
        if (hasRun) {
            addTransition("", "Run", runCond, 0.0f);
        }
        if (hasJump) {
            addTransition("",     "Jump", jumpCond, 0.0f);
            addTransition("Jump", "",     nullptr,  0.0f);
        }
    }
}

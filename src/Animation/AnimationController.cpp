// src/Animation/AnimationController.cpp

#include "AnimationController.h"
#include <cmath>
#include <iostream>

// ---------------------------------------------------------------------------
// Private helper: transition to a state identified by raw hash
// ---------------------------------------------------------------------------
static void setStateByHash(
    uint32_t                                            h,
    std::unordered_map<uint32_t, AnimationState>&       states_,
    std::unordered_map<uint32_t, std::string>&          stateNames_,
    std::string&                                        currentStateName_,
    uint32_t&                                           currentStateHash_,
    std::string&                                        previousStateName_,
    uint32_t&                                           previousStateHash_,
    bool&                                               blending_,
    float&                                              blendElapsed_,
    float&                                              blendDuration_,
    const std::vector<AnimationTransition>&             transitions_)
{
    if (h == currentStateHash_) return;

    if (h != 0 && states_.find(h) == states_.end()) {
        std::cerr << "[AnimationController] Unknown state hash: " << h << "\n";
        return;
    }

    previousStateName_ = currentStateName_;
    previousStateHash_ = currentStateHash_;
    currentStateHash_  = h;

    if (h == 0) {
        currentStateName_ = "";
        blending_         = false;
        blendElapsed_     = 0.0f;
        return;
    }

#ifndef NDEBUG
    auto it = stateNames_.find(h);
    currentStateName_ = (it != stateNames_.end()) ? it->second : std::to_string(h);
#else
    (void)stateNames_;
    currentStateName_ = std::to_string(h);
#endif

    states_[h].reset();

    blendDuration_ = 0.3f;
    for (const auto& t : transitions_) {
        if (t.from == previousStateHash_ && t.to == h) {
            blendDuration_ = t.blendDuration;
            break;
        }
    }
    blending_     = (previousStateHash_ != 0);
    blendElapsed_ = 0.0f;
}

// ---------------------------------------------------------------------------
// StringId-based primary API
// ---------------------------------------------------------------------------

void AnimationController::addState(StringId id, AnimationClip* clip,
                                    float speed, bool looping) {
    uint32_t h = id.value();
    states_[h] = AnimationState("", clip, speed, looping);
#ifndef NDEBUG
    const std::string* name = StringId::lookupName(h);
    if (name) stateNames_[h] = *name;
#endif
}

void AnimationController::addTransition(StringId from, StringId to,
                                         std::function<bool()> condition,
                                         float bDuration) {
    transitions_.push_back({ from.value(), to.value(), condition, bDuration });
}

void AnimationController::setState(StringId id) {
    setStateByHash(id.value(), states_, stateNames_,
                   currentStateName_, currentStateHash_,
                   previousStateName_, previousStateHash_,
                   blending_, blendElapsed_, blendDuration_, transitions_);
}

// ---------------------------------------------------------------------------
// update()
// ---------------------------------------------------------------------------

std::vector<glm::mat4> AnimationController::update(float deltaTime, Skeleton& skeleton) {
    // Check automatic transitions — handles bind-pose (hash=0) → movement and back.
    for (const auto& t : transitions_) {
        if (t.from == currentStateHash_ && t.condition && t.condition()) {
            setStateByHash(t.to, states_, stateNames_,
                           currentStateName_, currentStateHash_,
                           previousStateName_, previousStateHash_,
                           blending_, blendElapsed_, blendDuration_, transitions_);
            break;
        }
    }

    if (currentStateHash_ == 0) {
        return skeleton.computeBoneMatrices();
    }

    if (blending_ && previousStateHash_ != 0) {
        blendElapsed_ += deltaTime;
        float factor = AnimationBlender::crossfadeFactor(blendElapsed_, blendDuration_);

        AnimationState& prev = states_[previousStateHash_];
        AnimationState& curr = states_[currentStateHash_];

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

        if (factor >= 1.0f) blending_ = false;
    } else {
        states_[currentStateHash_].update(deltaTime, skeleton);
    }

    return skeleton.computeBoneMatrices();
}

// ---------------------------------------------------------------------------
// setupDefaultTransitions
// ---------------------------------------------------------------------------

void AnimationController::setupDefaultTransitions(std::function<bool()> walkCond,
                                                   std::function<bool()> runCond,
                                                   std::function<bool()> jumpCond) {
    constexpr StringId kIdle ("Idle");
    constexpr StringId kWalk ("Walk");
    constexpr StringId kRun  ("Run");
    constexpr StringId kJump ("Jump");
    constexpr StringId kBind;   // hash == 0 → bind pose

    auto notWalkNotRun = [walkCond, runCond]() { return !walkCond() && !runCond(); };
    auto walkOnly      = [walkCond]()           { return walkCond(); };

    bool hasIdle = states_.count(kIdle.value()) > 0;
    bool hasWalk = states_.count(kWalk.value()) > 0;
    bool hasRun  = states_.count(kRun.value())  > 0;
    bool hasJump = states_.count(kJump.value()) > 0;

    if (hasIdle && hasWalk) {
        addTransition(kIdle, kWalk, walkCond,      0.2f);
        addTransition(kWalk, kIdle, notWalkNotRun, 0.2f);
    }
    if (hasWalk && hasRun) {
        addTransition(kWalk, kRun,  runCond,  0.15f);
        addTransition(kRun,  kWalk, walkOnly, 0.15f);
    }
    if (hasIdle && hasJump) addTransition(kIdle, kJump, jumpCond, 0.1f);
    if (hasWalk && hasJump) addTransition(kWalk, kJump, jumpCond, 0.1f);
    if (hasRun  && hasJump) addTransition(kRun,  kJump, jumpCond, 0.1f);
    if (hasJump && hasIdle) addTransition(kJump, kIdle, nullptr,  0.2f);

    // For models with no Idle clip: bind-pose ↔ movement transitions.
    if (!hasIdle) {
        if (hasWalk) {
            addTransition(kBind, kWalk, walkCond,      0.0f);
            addTransition(kWalk, kBind, notWalkNotRun, 0.0f);
        }
        if (hasRun) {
            addTransition(kBind, kRun, runCond, 0.0f);
        }
        if (hasJump) {
            addTransition(kBind, kJump, jumpCond, 0.0f);
            addTransition(kJump, kBind, nullptr,  0.0f);
        }
    }
}

// src/Animation/AnimationController.h
// State machine managing animation transitions with crossfade blending.

#ifndef ENGINE_ANIMATIONCONTROLLER_H
#define ENGINE_ANIMATIONCONTROLLER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "AnimationState.h"
#include "AnimationBlender.h"
#include "Skeleton.h"

/// A transition between two named states.
struct AnimationTransition {
    std::string              from;
    std::string              to;
    std::function<bool()>    condition;      ///< Returns true when the transition should fire.
    float                    blendDuration;  ///< Crossfade seconds (0 = instant).
};

class AnimationController {
public:
    /// Register a state.  The controller does NOT own the clip pointer.
    void addState(const std::string& name, AnimationClip* clip,
                  float speed = 1.0f, bool looping = true);

    /// Define a transition.  condition may be nullptr (manual-only).
    void addTransition(const std::string& from, const std::string& to,
                       std::function<bool()> condition = nullptr,
                       float blendDuration = 0.3f);

    /// Trigger an immediate state change (with crossfade).
    void setState(const std::string& name);

    /// Advance the state machine and update skeleton bone transforms.
    /// Returns the final bone matrices ready for GPU upload.
    std::vector<glm::mat4> update(float deltaTime, Skeleton& skeleton);

    const std::string& getCurrentStateName() const { return currentStateName; }

    /// Set up common predefined transitions: Idle↔Walk↔Run, any→Jump, Jump→Idle.
    void setupDefaultTransitions(std::function<bool()> walkCond,
                                  std::function<bool()> runCond,
                                  std::function<bool()> jumpCond);

private:
    std::unordered_map<std::string, AnimationState> states;
    std::vector<AnimationTransition>                transitions;

    std::string currentStateName;
    std::string previousStateName;

    bool  blending       = false;
    float blendElapsed   = 0.0f;
    float blendDuration  = 0.3f;
};

#endif // ENGINE_ANIMATIONCONTROLLER_H

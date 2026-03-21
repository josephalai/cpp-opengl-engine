// src/Animation/AnimationController.h
// State machine managing animation transitions with crossfade blending.
// Internally keyed by hashed StringId (uint32_t) for hot-loop efficiency.
// The std::string-accepting API is kept as a convenience wrapper.

#ifndef ENGINE_ANIMATIONCONTROLLER_H
#define ENGINE_ANIMATIONCONTROLLER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>
#include "AnimationState.h"
#include "AnimationBlender.h"
#include "Skeleton.h"
#include "../Engine/StringId.h"

/// A transition between two named states (stored as hashed IDs).
struct AnimationTransition {
    uint32_t                 from;
    uint32_t                 to;
    std::function<bool()>    condition;      ///< Returns true when the transition should fire.
    float                    blendDuration;  ///< Crossfade seconds (0 = instant).
};

class AnimationController {
public:
    // ------------------------------------------------------------------
    // StringId API (preferred — zero-cost at runtime after initial hash)
    // ------------------------------------------------------------------

    /// Register a state keyed by StringId.
    void addState(StringId id, AnimationClip* clip,
                  float speed = 1.0f, bool looping = true);

    /// Define a transition keyed by StringId.
    void addTransition(StringId from, StringId to,
                       std::function<bool()> condition = nullptr,
                       float blendDuration = 0.3f);

    /// Trigger an immediate state change (with crossfade).
    void setState(StringId id);

    // ------------------------------------------------------------------
    // std::string convenience wrappers (hash at call-site)
    // ------------------------------------------------------------------
    void addState(const std::string& name, AnimationClip* clip,
                  float speed = 1.0f, bool looping = true) {
        StringId id(name);
        stateNames_[id.value()] = name;  // always track name so getCurrentStateName() works in all builds
        addState(id, clip, speed, looping);
    }
    void addTransition(const std::string& from, const std::string& to,
                       std::function<bool()> condition = nullptr,
                       float blendDuration = 0.3f) {
        // Empty string "" conventionally means "bind-pose" (hash=0)
        StringId f = from.empty() ? StringId() : StringId(from);
        StringId t = to.empty()   ? StringId() : StringId(to);
        addTransition(f, t, condition, blendDuration);
    }
    void setState(const std::string& name) {
        // Empty string "" → bind-pose (hash=0)
        setState(name.empty() ? StringId() : StringId(name));
    }

    /// Programmatically request a transition to the named state.
    /// Already-in-state calls are a no-op (setStateByHash guards against
    /// same-state re-entry).  Use this for remote/NPC entities whose
    /// animation state is driven by game logic rather than keyboard input.
    void requestTransition(const std::string& name) {
        setState(name.empty() ? StringId() : StringId(name));
    }

    // ------------------------------------------------------------------
    // Query & update
    // ------------------------------------------------------------------

    /// Advance the state machine and update skeleton bone transforms.
    /// Returns the final bone matrices ready for GPU upload.
    std::vector<glm::mat4> update(float deltaTime, Skeleton& skeleton);

    /// Returns the display name of the current state (empty = bind-pose).
    const std::string& getCurrentStateName() const { return currentStateName_; }

    /// Returns a sorted list of all registered state names.
    /// Names are populated when states are added via the std::string overload.
    std::vector<std::string> getStateNames() const;

    /// Set up common predefined transitions: Idle↔Walk↔Run, any→Jump, Jump→Idle.
    void setupDefaultTransitions(std::function<bool()> walkCond,
                                  std::function<bool()> runCond,
                                  std::function<bool()> jumpCond);

private:
    /// Core transition helper — sets state by hash without going through string→hash.
    void setStateByHash(uint32_t h);
    /// Internal state map keyed by StringId hash (uint32_t).
    std::unordered_map<uint32_t, AnimationState> states_;

    /// Reverse map: hash → display name.  Populated by the std::string overload of
    /// addState() in all builds so getCurrentStateName() always returns a readable name.
    std::unordered_map<uint32_t, std::string>    stateNames_;

    std::vector<AnimationTransition> transitions_;

    std::string currentStateName_;
    uint32_t    currentStateHash_  = 0;
    std::string previousStateName_;
    uint32_t    previousStateHash_ = 0;

    bool  blending_      = false;
    float blendElapsed_  = 0.0f;
    float blendDuration_ = 0.3f;

    /// Seconds elapsed since the last automatic (condition-driven) or programmatic
    /// state transition.  Used by update() to enforce kMinAutoTransitionInterval,
    /// preventing rapid Walk↔Idle flip-flops that restart animation clips.
    float timeSinceLastTransition_ = 0.0f;

    /// Minimum time (seconds) a state must be active before an automatic
    /// condition-driven transition may fire.  Programmatic requestTransition()
    /// calls (from game logic) bypass this guard so they always take effect.
    static constexpr float kMinAutoTransitionInterval = 0.1f;
};

#endif // ENGINE_ANIMATIONCONTROLLER_H

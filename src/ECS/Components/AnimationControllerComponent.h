// src/ECS/Components/AnimationControllerComponent.h
//
// ECS component for the modular (MMO-style) animation pipeline.
//
// In the modular pipeline, animations live in separate .glb files from the skin.
// AssetForge.py produces:
//   - skins/npc.glb   — mesh + skeleton only
//   - animations/idle.glb, animations/walk.glb … — skeleton + animation tracks only
//
// EntityFactory reads the JSON prefab's "AnimationController" block, calls
// AnimationLoader::loadExternalAnimation() for each entry, and populates this
// component.  The AnimatedModelComponent's AnimationController state machine is
// then seeded from these shared clips so the existing rendering path is unchanged.
//
// Lifetime: the shared_ptrs keep each AnimationClip alive for as long as any
// component (or AnimationController state) holds a reference to it.

#ifndef ECS_ANIMATIONCONTROLLERCOMPONENT_H
#define ECS_ANIMATIONCONTROLLERCOMPONENT_H

#include <memory>
#include <string>
#include <unordered_map>
#include "../../Animation/AnimationClip.h"

struct AnimationControllerComponent {
    /// All externally-loaded animation clips, keyed by logical state name
    /// (e.g. "idle", "walk", "run").  These are the clips registered into
    /// the AnimationController state machine stored on AnimatedModelComponent.
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> animations;

    /// The state that was set as default at load time (from "default_state" in JSON).
    std::string defaultState;

    /// Tracks the currently active animation state name (mirrors
    /// AnimationController::getCurrentStateName() for external query).
    std::string currentAnimationName;

    /// Accumulated playback time in seconds for the current clip.
    /// Reset to 0 whenever currentAnimationName changes.
    float playbackTime = 0.0f;
};

#endif // ECS_ANIMATIONCONTROLLERCOMPONENT_H

// src/Animation/AnimationClip.h
// Holds all keyframe channels for every bone across one animation.

#ifndef ENGINE_ANIMATIONCLIP_H
#define ENGINE_ANIMATIONCLIP_H

#include <string>
#include <map>
#include <vector>
#include <cmath>
#include "Bone.h"
#include "Skeleton.h"

/// All keyframe data for one bone within a clip.
struct BoneAnimation {
    std::vector<KeyPosition> positions;
    std::vector<KeyRotation> rotations;
    std::vector<KeyScale>    scales;
};

class AnimationClip {
public:
    std::string name;
    float       duration;         ///< Total duration in ticks.
    float       ticksPerSecond;   ///< Ticks per real second.

    /// Per-bone keyframe channels, keyed by bone name.
    std::map<std::string, BoneAnimation> channels;

    AnimationClip() : duration(0.0f), ticksPerSecond(25.0f) {}
    AnimationClip(const std::string& n, float dur, float tps)
        : name(n), duration(dur), ticksPerSecond(tps) {}

    float getDurationSeconds() const {
        return (ticksPerSecond > 0.0f) ? duration / ticksPerSecond : duration;
    }

    /// Advance skeleton bone transforms to the given time in seconds.
    void interpolate(float timeSeconds, Skeleton& skeleton) const;
};

#endif // ENGINE_ANIMATIONCLIP_H

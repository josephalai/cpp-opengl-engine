// src/Animation/AnimationClip.cpp

#include "AnimationClip.h"

void AnimationClip::interpolate(float timeSeconds, Skeleton& skeleton) const {
    float tps  = (ticksPerSecond > 0.0f) ? ticksPerSecond : 25.0f;
    float tick = timeSeconds * tps;
    tick = std::fmod(tick, duration);
    if (tick < 0.0f) tick += duration;

    for (Bone* bone : skeleton.bones) {
        auto it = channels.find(bone->name);
        if (it != channels.end()) {
            const BoneAnimation& ba = it->second;
            bone->update(tick, ba.positions, ba.rotations, ba.scales);
        }
    }
}

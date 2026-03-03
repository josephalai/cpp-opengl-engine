// src/Animation/AnimationBlender.cpp

#include "AnimationBlender.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// Helper: decompose a mat4 into translation + rotation + scale
static void decompose(const glm::mat4& m, glm::vec3& t, glm::quat& r, glm::vec3& s) {
    t = glm::vec3(m[3]);
    s.x = glm::length(glm::vec3(m[0]));
    s.y = glm::length(glm::vec3(m[1]));
    s.z = glm::length(glm::vec3(m[2]));
    glm::mat3 rot(m[0] / s.x, m[1] / s.y, m[2] / s.z);
    r = glm::normalize(glm::quat_cast(rot));
}

static glm::mat4 compose(const glm::vec3& t, const glm::quat& r, const glm::vec3& s) {
    return glm::translate(glm::mat4(1.0f), t)
         * glm::toMat4(r)
         * glm::scale(glm::mat4(1.0f), s);
}

void AnimationBlender::blend(const AnimationClip& clipA, float timeA,
                               const AnimationClip& clipB, float timeB,
                               float factor, Skeleton& skeleton) {
    factor = std::clamp(factor, 0.0f, 1.0f);

    // Evaluate both clips independently into temporary skeletons,
    // then blend the resulting bone local transforms.

    // We directly blend at the Bone::localTransform level.
    // First, let clipA write all transforms.
    clipA.interpolate(timeA, skeleton);

    // Save transforms from clipA.
    std::vector<glm::mat4> transformsA(skeleton.bones.size());
    for (int i = 0; i < static_cast<int>(skeleton.bones.size()); ++i)
        transformsA[i] = skeleton.bones[i]->localTransform;

    // Let clipB write all transforms.
    clipB.interpolate(timeB, skeleton);

    // Blend A and B
    for (int i = 0; i < static_cast<int>(skeleton.bones.size()); ++i) {
        const glm::mat4& mA = transformsA[i];
        const glm::mat4& mB = skeleton.bones[i]->localTransform;

        glm::vec3 tA, sA, tB, sB;
        glm::quat rA, rB;
        decompose(mA, tA, rA, sA);
        decompose(mB, tB, rB, sB);

        glm::vec3 t = glm::mix(tA, tB, factor);
        glm::quat r = glm::normalize(glm::slerp(rA, rB, factor));
        glm::vec3 s = glm::mix(sA, sB, factor);

        skeleton.bones[i]->localTransform = compose(t, r, s);
    }
}

float AnimationBlender::crossfadeFactor(float elapsed, float blendDuration) {
    if (blendDuration <= 0.0f) return 1.0f;
    return std::clamp(elapsed / blendDuration, 0.0f, 1.0f);
}

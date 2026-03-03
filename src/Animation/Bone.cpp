// src/Animation/Bone.cpp

#include "Bone.h"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <cmath>

Bone::Bone(const std::string& n, int i, const glm::mat4& offset)
    : name(n), id(i), offsetMatrix(offset), localTransform(glm::mat4(1.0f)) {}

int Bone::findIndex(const std::vector<float>& times, float t) {
    if (times.size() < 2) return 0;
    for (int i = 0; i < static_cast<int>(times.size()) - 1; ++i) {
        if (t < times[i + 1]) return i;
    }
    return static_cast<int>(times.size()) - 2;
}

float Bone::blendFactor(float last, float next, float t) {
    if (next <= last) return 0.0f;
    return (t - last) / (next - last);
}

glm::vec3 Bone::interpolatePosition(float t, const std::vector<KeyPosition>& keys) {
    if (keys.size() == 1) return keys[0].value;
    std::vector<float> times;
    times.reserve(keys.size());
    for (const auto& k : keys) times.push_back(k.time);
    int idx = findIndex(times, t);
    float f = blendFactor(times[idx], times[idx + 1], t);
    return glm::mix(keys[idx].value, keys[idx + 1].value, f);
}

glm::quat Bone::interpolateRotation(float t, const std::vector<KeyRotation>& keys) {
    if (keys.size() == 1) return glm::normalize(keys[0].value);
    std::vector<float> times;
    times.reserve(keys.size());
    for (const auto& k : keys) times.push_back(k.time);
    int idx = findIndex(times, t);
    float f = blendFactor(times[idx], times[idx + 1], t);
    return glm::normalize(glm::slerp(keys[idx].value, keys[idx + 1].value, f));
}

glm::vec3 Bone::interpolateScale(float t, const std::vector<KeyScale>& keys) {
    if (keys.size() == 1) return keys[0].value;
    std::vector<float> times;
    times.reserve(keys.size());
    for (const auto& k : keys) times.push_back(k.time);
    int idx = findIndex(times, t);
    float f = blendFactor(times[idx], times[idx + 1], t);
    return glm::mix(keys[idx].value, keys[idx + 1].value, f);
}

void Bone::update(float animTime,
                  const std::vector<KeyPosition>& positions,
                  const std::vector<KeyRotation>& rotations,
                  const std::vector<KeyScale>&    scales) {
    glm::mat4 T = glm::mat4(1.0f);
    if (!positions.empty())
        T = glm::translate(glm::mat4(1.0f), interpolatePosition(animTime, positions));

    glm::mat4 R = glm::mat4(1.0f);
    if (!rotations.empty())
        R = glm::toMat4(interpolateRotation(animTime, rotations));

    glm::mat4 S = glm::mat4(1.0f);
    if (!scales.empty())
        S = glm::scale(glm::mat4(1.0f), interpolateScale(animTime, scales));

    localTransform = T * R * S;
}

// src/Animation/Bone.h
// Represents a single bone in a skeletal hierarchy.

#ifndef ENGINE_BONE_H
#define ENGINE_BONE_H

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/// Position keyframe.
struct KeyPosition {
    glm::vec3 value;
    float time;
};

/// Rotation keyframe (quaternion).
struct KeyRotation {
    glm::quat value;
    float time;
};

/// Scale keyframe.
struct KeyScale {
    glm::vec3 value;
    float time;
};

class Bone {
public:
    std::string name;
    int         id;
    glm::mat4   offsetMatrix;    ///< Bind-pose inverse (object→bone space).
    glm::mat4   localTransform;  ///< Updated each frame by interpolate().

    std::vector<Bone*> children;

    Bone(const std::string& name, int id, const glm::mat4& offsetMatrix);

    /// Interpolate localTransform from keyframe channels at animTime (in ticks).
    void update(float animTime,
                const std::vector<KeyPosition>& positions,
                const std::vector<KeyRotation>& rotations,
                const std::vector<KeyScale>&    scales);

private:
    static int   findIndex(const std::vector<float>& times, float t);
    static float blendFactor(float last, float next, float t);

    glm::vec3 interpolatePosition(float t, const std::vector<KeyPosition>& keys);
    glm::quat interpolateRotation(float t, const std::vector<KeyRotation>& keys);
    glm::vec3 interpolateScale   (float t, const std::vector<KeyScale>&    keys);
};

#endif // ENGINE_BONE_H

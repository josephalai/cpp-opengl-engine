// src/Animation/Skeleton.h
// Owns the full bone hierarchy and computes per-frame bone matrices for the GPU.

#ifndef ENGINE_SKELETON_H
#define ENGINE_SKELETON_H

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Bone.h"

static const int MAX_BONES = 100;

class Skeleton {
public:
    Bone*                                   root = nullptr;
    std::vector<Bone*>                      bones;       ///< Flat array indexed by bone ID.
    std::unordered_map<std::string, Bone*>  bonesByName;

    int    getBoneCount()                      const { return static_cast<int>(bones.size()); }
    Bone*  getBone(int id)                     const;
    Bone*  getBoneByName(const std::string& n) const;

    /// Walk the hierarchy, accumulate parent transforms, apply offset matrix.
    /// Returns up to MAX_BONES matrices ready for upload to a uniform array.
    std::vector<glm::mat4> computeBoneMatrices() const;

    ~Skeleton();

private:
    void computeRecursive(Bone* bone, const glm::mat4& parentTransform,
                          std::vector<glm::mat4>& out) const;
};

#endif // ENGINE_SKELETON_H

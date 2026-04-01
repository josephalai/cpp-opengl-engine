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

    /// Accumulated transform from non-bone ancestor nodes (e.g. Scene Root,
    /// Armature) that sit above the root bone in the Assimp node hierarchy.
    /// buildBoneHierarchy() populates this; computeBoneMatrices() uses it as
    /// the initial parent transform so bone matrices are in the correct
    /// coordinate space.  For models whose Armature node carries a -90° X
    /// rotation (common in Blender/Meshy exports to convert Z-up → Y-up),
    /// this ensures the model stands upright without a manual model_rotation.
    glm::mat4 rootTransform = glm::mat4(1.0f);

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

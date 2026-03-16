// src/Animation/Skeleton.cpp

#include "Skeleton.h"

Bone* Skeleton::getBone(int id) const {
    if (id < 0 || id >= static_cast<int>(bones.size())) return nullptr;
    return bones[id];
}

Bone* Skeleton::getBoneByName(const std::string& name) const {
    auto it = bonesByName.find(name);
    return it != bonesByName.end() ? it->second : nullptr;
}

std::vector<glm::mat4> Skeleton::computeBoneMatrices() const {
    std::vector<glm::mat4> result(bones.size(), glm::mat4(1.0f));
    if (root) computeRecursive(root, rootTransform, result);
    return result;
}

void Skeleton::computeRecursive(Bone* bone, const glm::mat4& parentTransform,
                                  std::vector<glm::mat4>& out) const {
    glm::mat4 globalTransform = parentTransform * bone->nonBoneParentTransform * bone->localTransform;
    if (bone->id >= 0 && bone->id < static_cast<int>(out.size())) {
        out[bone->id] = globalTransform * bone->offsetMatrix;
    }
    for (Bone* child : bone->children) {
        computeRecursive(child, globalTransform, out);
    }
}

Skeleton::~Skeleton() {
    for (Bone* b : bones) delete b;
    bones.clear();
    bonesByName.clear();
    root = nullptr;
}

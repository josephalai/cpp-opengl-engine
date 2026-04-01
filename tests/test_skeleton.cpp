// tests/test_skeleton.cpp
// Unit tests for Skeleton::rootTransform accumulation and computeBoneMatrices().

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>
#include "../src/Animation/Skeleton.h"
#include "../src/Animation/Bone.h"

// Helper: check that two matrices are approximately equal.
static bool matApproxEq(const glm::mat4& a, const glm::mat4& b, float eps = 1e-4f) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (std::abs(a[c][r] - b[c][r]) > eps)
                return false;
    return true;
}

class SkeletonTest : public ::testing::Test {
protected:
    // Build a minimal 2-bone skeleton: root → child.
    void SetUp() override {
        // Root bone (id=0): identity bind pose.
        glm::mat4 rootOffset = glm::inverse(glm::mat4(1.0f));  // identity
        rootBone = new Bone("root", 0, rootOffset);
        rootBone->localTransform = glm::mat4(1.0f);

        // Child bone (id=1): translated 1 unit along Y.
        glm::mat4 childBindPose = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0));
        glm::mat4 childOffset = glm::inverse(childBindPose);
        childBone = new Bone("child", 1, childOffset);
        childBone->localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0));

        rootBone->children.push_back(childBone);

        skeleton.root = rootBone;
        skeleton.bones.push_back(rootBone);
        skeleton.bones.push_back(childBone);
        skeleton.bonesByName["root"] = rootBone;
        skeleton.bonesByName["child"] = childBone;
    }

    Skeleton skeleton;
    Bone* rootBone = nullptr;
    Bone* childBone = nullptr;
};

TEST_F(SkeletonTest, IdentityRootTransform_ProducesIdentityBoneMatrices) {
    // With identity rootTransform and identity bind-pose, bone matrices should
    // be identity (globalTransform * offsetMatrix = I * I^-1 = I).
    skeleton.rootTransform = glm::mat4(1.0f);
    auto matrices = skeleton.computeBoneMatrices();

    ASSERT_EQ(matrices.size(), 2u);
    EXPECT_TRUE(matApproxEq(matrices[0], glm::mat4(1.0f)))
        << "Root bone matrix should be identity in rest pose";
    EXPECT_TRUE(matApproxEq(matrices[1], glm::mat4(1.0f)))
        << "Child bone matrix should be identity in rest pose";
}

TEST_F(SkeletonTest, NonIdentityRootTransform_IsApplied) {
    // If we set rootTransform to a rotation, all bone matrices should reflect it.
    // With rootTransform R:
    //   boneMatrix = (R * localTransformChain) * offsetMatrix
    //              = R * globalTransform_bind * inverse(globalTransform_bind)
    //   In bind pose this simplifies to R.
    // So root bone matrix = R * I * I^-1 = R
    glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
                               glm::vec3(1.0f, 0.0f, 0.0f));
    skeleton.rootTransform = R;
    auto matrices = skeleton.computeBoneMatrices();

    ASSERT_EQ(matrices.size(), 2u);
    // Root: (R * localTransform_root) * offsetMatrix_root
    //     = (R * I) * I = R
    EXPECT_TRUE(matApproxEq(matrices[0], R))
        << "Root bone matrix should equal the rootTransform rotation";
}

TEST_F(SkeletonTest, DefaultRootTransform_IsIdentity) {
    // Verify that the default rootTransform is identity.
    Skeleton fresh;
    EXPECT_TRUE(matApproxEq(fresh.rootTransform, glm::mat4(1.0f)));
}

TEST_F(SkeletonTest, BoneCount) {
    EXPECT_EQ(skeleton.getBoneCount(), 2);
}

TEST_F(SkeletonTest, GetBoneByName) {
    EXPECT_EQ(skeleton.getBoneByName("root"), rootBone);
    EXPECT_EQ(skeleton.getBoneByName("child"), childBone);
    EXPECT_EQ(skeleton.getBoneByName("nonexistent"), nullptr);
}

TEST_F(SkeletonTest, GetBoneById) {
    EXPECT_EQ(skeleton.getBone(0), rootBone);
    EXPECT_EQ(skeleton.getBone(1), childBone);
    EXPECT_EQ(skeleton.getBone(-1), nullptr);
    EXPECT_EQ(skeleton.getBone(5), nullptr);
}

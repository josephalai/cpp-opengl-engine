// tests/test_animation_controller.cpp
// Unit tests for AnimationController state machine.

#include <gtest/gtest.h>
#include "../src/Animation/AnimationController.h"
#include "../src/Animation/AnimationClip.h"
#include "../src/Animation/Skeleton.h"
#include "../src/Animation/Bone.h"

// Minimal AnimationClip with a single-keyframe channel (just for testing).
static AnimationClip makeMinimalClip(const std::string& name, float duration = 1.0f) {
    AnimationClip clip;
    clip.name = name;
    clip.duration = duration;
    clip.ticksPerSecond = 1.0f;
    return clip;
}

class AnimationControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        idleClip = makeMinimalClip("Idle", 2.0f);
        walkClip = makeMinimalClip("Walk", 1.5f);
        runClip  = makeMinimalClip("Run",  1.0f);

        // Minimal skeleton with one bone.
        glm::mat4 offset = glm::inverse(glm::mat4(1.0f));
        rootBone = new Bone("root", 0, offset);
        rootBone->localTransform = glm::mat4(1.0f);
        skeleton.root = rootBone;
        skeleton.bones.push_back(rootBone);
        skeleton.bonesByName["root"] = rootBone;
    }

    AnimationClip idleClip, walkClip, runClip;
    Skeleton skeleton;
    Bone* rootBone = nullptr;
};

TEST_F(AnimationControllerTest, AddState_RegistersStates) {
    AnimationController ctrl;
    ctrl.addState("Idle", &idleClip);
    ctrl.addState("Walk", &walkClip);
    ctrl.addState("Run",  &runClip);

    auto names = ctrl.getStateNames();
    EXPECT_EQ(names.size(), 3u);
    EXPECT_NE(std::find(names.begin(), names.end(), "Idle"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Walk"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Run"),  names.end());
}

TEST_F(AnimationControllerTest, SetState_ChangesCurrentState) {
    AnimationController ctrl;
    ctrl.addState("Idle", &idleClip);
    ctrl.addState("Walk", &walkClip);

    ctrl.setState("Idle");
    EXPECT_EQ(ctrl.getCurrentStateName(), "Idle");

    ctrl.setState("Walk");
    EXPECT_EQ(ctrl.getCurrentStateName(), "Walk");
}

TEST_F(AnimationControllerTest, RequestTransition_ChangesState) {
    AnimationController ctrl;
    ctrl.addState("Idle", &idleClip);
    ctrl.addState("Walk", &walkClip);

    ctrl.setState("Idle");
    EXPECT_EQ(ctrl.getCurrentStateName(), "Idle");

    ctrl.requestTransition("Walk");
    EXPECT_EQ(ctrl.getCurrentStateName(), "Walk");
}

TEST_F(AnimationControllerTest, RequestTransition_InvalidState_NoChange) {
    AnimationController ctrl;
    ctrl.addState("Idle", &idleClip);
    ctrl.setState("Idle");

    ctrl.requestTransition("NonExistent");
    EXPECT_EQ(ctrl.getCurrentStateName(), "Idle");
}

TEST_F(AnimationControllerTest, Update_ReturnsBoneMatrices) {
    AnimationController ctrl;
    ctrl.addState("Idle", &idleClip);
    ctrl.setState("Idle");

    auto matrices = ctrl.update(0.016f, skeleton);
    EXPECT_EQ(matrices.size(), 1u);  // one bone
}

TEST_F(AnimationControllerTest, GetStateNames_SortedAlphabetically) {
    AnimationController ctrl;
    ctrl.addState("Walk", &walkClip);
    ctrl.addState("Idle", &idleClip);
    ctrl.addState("Run",  &runClip);

    auto names = ctrl.getStateNames();
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "Idle");
    EXPECT_EQ(names[1], "Run");
    EXPECT_EQ(names[2], "Walk");
}

TEST_F(AnimationControllerTest, SetupDefaultTransitions_IdleWalkRun) {
    AnimationController ctrl;
    ctrl.addState("Idle", &idleClip);
    ctrl.addState("Walk", &walkClip);
    ctrl.addState("Run",  &runClip);
    ctrl.setState("Idle");

    bool isWalking = false;
    bool isRunning = false;

    ctrl.setupDefaultTransitions(
        [&]() { return isWalking; },
        [&]() { return isRunning; },
        nullptr);

    // Initially Idle — update should keep Idle.
    ctrl.update(0.2f, skeleton);  // advance past min interval
    EXPECT_EQ(ctrl.getCurrentStateName(), "Idle");

    // Start walking → transition Idle→Walk.
    isWalking = true;
    ctrl.update(0.2f, skeleton);
    EXPECT_EQ(ctrl.getCurrentStateName(), "Walk");

    // Stop walking → transition Walk→Idle.
    isWalking = false;
    ctrl.update(0.2f, skeleton);
    EXPECT_EQ(ctrl.getCurrentStateName(), "Idle");
}

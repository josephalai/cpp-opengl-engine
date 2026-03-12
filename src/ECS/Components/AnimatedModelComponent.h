#ifndef ECS_ANIMATEDMODELCOMPONENT_H
#define ECS_ANIMATEDMODELCOMPONENT_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../../Animation/AnimatedModel.h"
#include "../../Animation/AnimationController.h"

/// ECS component that holds all state for a skeletal-mesh character.
/// Replaces the legacy AnimatedEntity struct for the ECS rendering pipeline.
///
/// - Local-player entities: isLocalPlayer=true, ownsModel=true.
///   The AnimationSystem syncs position/rotation from the physics-driven Player*.
/// - Remote entities: isLocalPlayer=false.
///   Position/rotation is driven by NetworkInterpolationSystem via TransformComponent.
///
/// The AnimationSystem collects all entities with this component each frame via
/// registry.view<AnimatedModelComponent, TransformComponent>() and delegates to
/// AnimatedRenderer for skinned-mesh rendering — no legacy
/// std::vector<AnimatedEntity*> needed.
struct AnimatedModelComponent {
    AnimatedModel*       model       = nullptr;  ///< Loaded skeletal model (geometry + clips)
    AnimationController* controller  = nullptr;  ///< State machine for clip selection
    /// Pure visual offset applied on top of position at render time.
    /// Does not affect physics. Use Up/Down arrows at runtime to find the
    /// correct value, then bake it into scene.json or the prefab.
    glm::vec3            modelOffset = glm::vec3(0.0f);
    /// Per-prefab model-space rotation correction applied before the coordinate
    /// correction from Assimp. Used to fix models that load sideways or on their
    /// stomachs due to differing coordinate conventions in the source .glb/.fbx.
    /// Set from the prefab's AnimatedModelComponent.model_rotation JSON field
    /// (Euler angles in degrees, XYZ order). Identity by default (no correction).
    glm::mat4            modelRotationMat = glm::mat4(1.0f);
    float                scale       = 1.0f;
    /// True only for the local client's own character (marked by Engine after load).
    /// False for remote entities so AnimationSystem does not overwrite their
    /// transforms with the local player's physics position.
    bool                 isLocalPlayer = false;
    /// True when this component is responsible for deleting the model on cleanup.
    /// Local-player entities own their model. Remote entities loaded via
    /// EntityFactory also own their model (each gets its own loaded copy).
    bool                 ownsModel   = false;
};

#endif // ECS_ANIMATEDMODELCOMPONENT_H

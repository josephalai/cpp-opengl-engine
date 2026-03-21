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
    glm::vec3            lastPosition = glm::vec3(0.0f);
    glm::vec3            modelOffset = glm::vec3(0.0f);
    /// Per-prefab model-space rotation correction applied in place of the
    /// automatic coordinateCorrection from the Assimp loader.  Used to fix
    /// models that load sideways or on their stomachs due to differing
    /// coordinate conventions in the source .glb/.fbx.
    ///
    /// **Initialisation rules** (must be maintained by every creation site):
    ///   - EntityFactory, Engine (legacy promotion), SceneLoaderJson:
    ///     default to `animModel->coordinateCorrection` so Y-up auto-correction
    ///     is preserved when no explicit override is provided.
    ///   - If the prefab specifies `components.AnimatedModelComponent.model_rotation`,
    ///     EntityFactory REPLACES this field with the user-specified rotation.
    ///
    /// AnimatedRenderer multiplies ONLY this matrix (not coordinateCorrection)
    /// so the two corrections are never compounded.
    glm::mat4            modelRotationMat = glm::mat4(1.0f);
    float                autoWalkYaw = 0.0f;
    bool                 useAutoWalkYaw = false;
    /// Last yaw (degrees) set by keyboard input in PlayerMovementSystem.
    /// Preserved across server reconciliation snap-backs so AnimationSystem
    /// does not derive a backward-facing direction from the snap-back delta.
    float                lastInputYaw = 0.0f;
    /// Set to true by NetworkSystem whenever a hard reconciliation warp
    /// occurs.  AnimationSystem clears it after absorbing the event so the
    /// snap-back position delta is never treated as actual locomotion.
    bool                 wasSnappedBack = false;
    /// False on the very first frame so AnimationSystem can initialise
    /// lastPosition to the actual spawn position instead of (0,0,0).
    /// Prevents the large one-frame delta artifact at startup.
    bool                 lastPositionInitialized = false;
    /// Set to true by NetworkSystem while a smooth server-authoritative LERP
    /// is in progress (hasReconcileTarget_).  While true AND a movement key is
    /// held, AnimationSystem treats the player as moving regardless of the
    /// position delta (which points toward the reconcile target, not the key
    /// direction), preventing the walk↔idle flip-flop stutter during auto-walk
    /// and post-spawn reconciliation.
    bool                 suppressDeltaAnimation = false;
    /// True when the entity is actually moving (any keyboard direction OR
    /// auto-walk via click-to-walk).  Set each frame by AnimationSystem.
    /// The AnimationController walk-condition lambda reads this flag so that
    /// the Walk animation fires for all directions and click-to-walk, not
    /// just when W is held.
    bool                 isMoving = false;
    /// Hysteresis timer (seconds) that keeps isMoving=true for a short window
    /// after the last detected movement input or position delta.  Prevents
    /// single-frame deltaSq flicker (especially during warpPlayer reconciliation)
    /// from rapidly toggling Walk↔Idle and restarting the animation clip.
    float                movingTimer = 0.0f;
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

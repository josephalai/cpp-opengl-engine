// src/RenderEngine/AnimatedRenderer.h
// Renders AnimatedModel instances — uploads bone matrices each frame.

#ifndef ENGINE_ANIMATEDRENDERER_H
#define ENGINE_ANIMATEDRENDERER_H

#include <vector>
#include <map>
#include <glm/glm.hpp>
#include "../Animation/AnimatedModel.h"
#include "../Animation/AnimationController.h"
#include "../Shaders/AnimatedShader.h"
#include "../Toolbox/Maths.h"
#include "../Entities/Light.h"
#include "../Entities/Camera.h"

class Entity;  // Forward declaration — AnimatedEntity holds a non-owning pointer.

/// An in-scene animated character (model + controller + transform).
struct AnimatedEntity {
    AnimatedModel*       model         = nullptr;
    AnimationController* controller    = nullptr;
    glm::vec3            position      = glm::vec3(0.0f);
    glm::vec3            rotation      = glm::vec3(0.0f);
    float                scale         = 1.0f;
    /// Pure visual offset applied on top of position at render time.
    /// Does not affect physics. Use Up/Down arrows at runtime to find the
    /// correct value, then bake it into scene.cfg or the constructor.
    glm::vec3            modelOffset   = glm::vec3(0.0f);
    /// Authoritative model-space rotation correction.  Defaults to the loader's
    /// coordinateCorrection; overridden by the prefab's model_rotation when set.
    /// AnimatedRenderer uses ONLY this matrix — coordinateCorrection is not
    /// multiplied again so that the two corrections are never compounded.
    glm::mat4            modelRotationMat = glm::mat4(1.0f);
    /// True only for the local client's own character (loaded via SceneLoader).
    /// Remote animated entities leave this false so AnimationSystem does not
    /// overwrite their positions with the local player's physics transform.
    bool                 isLocalPlayer = false;
    /// True when this entity is responsible for deleting its model on cleanup.
    /// Local-player entities own their model (set to true by SceneLoader).
    /// Remote entities share the local player's model pointer and must NOT
    /// delete it (set to false by Engine::onNetworkSpawn) to prevent double-free.
    bool                 ownsModel     = false;
    /// For remote entities only: pointer to the Entity that carries the
    /// NetworkSyncComponent driving this animated character's world position.
    /// Null for local-player entities.
    Entity*              pairedEntity  = nullptr;

    // --- Modular Equipment System ---
    /// When true, activeMeshes is used instead of model->meshes.
    bool                                   isModular    = false;
    /// Pre-built list of mesh pointers to render (from nakedParts + equippedArmor).
    /// Only populated when isModular == true.
    std::vector<const AnimatedMesh*>       activeMeshes;
};

class AnimatedRenderer {
public:
    explicit AnimatedRenderer(AnimatedShader* shader);
    ~AnimatedRenderer();

    /// Render a list of animated entities.
    void render(const std::vector<AnimatedEntity*>& entities,
                float deltaTime,
                const std::vector<Light*>& lights,
                Camera* camera,
                const glm::mat4& projectionMatrix);

private:
    AnimatedShader* shader;

    /// 1×1 white RGBA texture used as a fallback when a mesh has no texture
    /// (textureID == 0).  Without this, the fragment shader's alpha-discard
    /// (`if (texColor.a < 0.5) discard`) receives alpha=0 from an unbound
    /// sampler and kills every fragment, making the model invisible.
    GLuint fallbackTextureID_ = 0;

    void renderMesh(const AnimatedMesh& mesh,
                    const std::vector<glm::mat4>& boneMatrices);
};

#endif // ENGINE_ANIMATEDRENDERER_H

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

/// An in-scene animated character (model + controller + transform).
struct AnimatedEntity {
    AnimatedModel*       model       = nullptr;
    AnimationController* controller  = nullptr;
    glm::vec3            position    = glm::vec3(0.0f);
    glm::vec3            rotation    = glm::vec3(0.0f);
    float                scale       = 1.0f;
    /// Pure visual offset applied on top of position at render time.
    /// Does not affect physics. Use Up/Down arrows at runtime to find the
    /// correct value, then bake it into scene.cfg or the constructor.
    glm::vec3            modelOffset = glm::vec3(0.0f);
    /// true for the local player's entity (position driven by Player/physics).
    /// false for remote entities (position driven by NetworkSyncComponent).
    bool                 isLocal     = true;
    /// true when this entity owns its AnimatedModel and should delete it on
    /// cleanup.  Remote entities share a cached model and must NOT delete it.
    bool                 ownsModel   = true;
};

class AnimatedRenderer {
public:
    explicit AnimatedRenderer(AnimatedShader* shader);

    /// Render a list of animated entities.
    void render(const std::vector<AnimatedEntity*>& entities,
                float deltaTime,
                const std::vector<Light*>& lights,
                Camera* camera,
                const glm::mat4& projectionMatrix);

private:
    AnimatedShader* shader;

    void renderMesh(const AnimatedMesh& mesh,
                    const std::vector<glm::mat4>& boneMatrices);
};

#endif // ENGINE_ANIMATEDRENDERER_H

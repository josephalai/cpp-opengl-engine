// ShadowMap.h — depth FBO used for directional shadow mapping.

#ifndef ENGINE_SHADOWMAP_H
#define ENGINE_SHADOWMAP_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <entt/entt.hpp>

#include "../Entities/Entity.h"
#include "../Entities/Light.h"
#include "../Toolbox/Maths.h"
#include "ShadowShader.h"
#include "../ECS/Components/StaticModelComponent.h"
#include "../ECS/Components/TransformComponent.h"

/// A depth-only off-screen render target for directional shadow mapping.
class ShadowMap {
public:
    static constexpr int kDefaultSize = 2048;

    /// Create a shadow map of given resolution.
    explicit ShadowMap(int size = kDefaultSize);
    ~ShadowMap();

    /// Bind the depth FBO so subsequent draws write into the shadow map.
    void bindForWriting() const;

    /// Restore the default framebuffer.
    void unbind(int displayWidth, int displayHeight) const;

    /// Compute light-space matrix from a directional light and scene bounds.
    /// @param lightDir   normalised direction FROM scene TO light
    /// @param viewCenter approximate centre of the visible scene (e.g. camera position)
    /// @param radius     half-size of the orthographic frustum
    glm::mat4 computeLightSpaceMatrix(const glm::vec3& lightDir,
                                      const glm::vec3& viewCenter,
                                      float            radius = 300.0f) const;

    /// Render entities into the shadow map using the shadow shader.
    void renderShadowMap(const std::vector<Entity*>& entities,
                         const glm::mat4&            lightSpaceMatrix,
                         ShadowShader*               shader) const;

    /// ECS-based shadow pass: renders Player + StaticModelComponent entities.
    void renderShadowMapFromRegistry(entt::registry&    registry,
                                     Entity*             player,
                                     const glm::mat4&   lightSpaceMatrix,
                                     ShadowShader*       shader) const;

    /// OpenGL texture ID of the depth texture (bind this for PCF sampling).
    GLuint getDepthTexture() const { return depthTexture_; }

    int getSize() const { return size_; }

private:
    int    size_;
    GLuint fbo_;
    GLuint depthTexture_;

    void init();
};

#endif // ENGINE_SHADOWMAP_H

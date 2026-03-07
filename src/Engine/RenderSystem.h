// src/Engine/RenderSystem.h
// Subsystem that owns the primary rendering pass: shadow map, scene, FBO (reflection),
// bounding box picking, instanced geometry, and water.  Wraps MasterRenderer.
// Applies view-frustum culling before submitting geometry to MasterRenderer.
//
// Phase 2 Step 3 — Mandate 3 + 4: Pure ECS with ActiveChunkTag.
//   All render lists are built from registry.view<> queries each frame.
//   Only entities tagged with ActiveChunkTag (by StreamingSystem) are rendered.
//   No side-vectors, no ChunkManager* passed as constructor argument.

#ifndef ENGINE_RENDERSYSTEM_H
#define ENGINE_RENDERSYSTEM_H

#include "ISystem.h"
#include "../Culling/FrustumCuller.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

class MasterRenderer;
class FrameBuffers;
class Camera;
class InstancedModel;

class RenderSystem : public ISystem {
public:
    /// @param registry          Engine-level ECS registry — sole source of scene data.
    /// @param instancedModel    Optional instanced-draw model (e.g. 500 trees).
    RenderSystem(MasterRenderer*  renderer,
                 FrameBuffers*    reflectFbo,
                 entt::registry&  registry,
                 Camera*          camera,
                 const glm::mat4& projectionMatrix,
                 InstancedModel*  instancedModel = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    MasterRenderer*  renderer_;
    FrameBuffers*    reflectFbo_;
    entt::registry&  registry_;
    Camera*          camera_;
    glm::mat4        projectionMatrix_;
    FrustumCuller    culler_;
    InstancedModel*  instancedModel_;
};

#endif // ENGINE_RENDERSYSTEM_H

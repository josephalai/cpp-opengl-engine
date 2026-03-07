// src/Engine/RenderSystem.h
// Subsystem that owns the primary rendering pass: shadow map, scene, FBO (reflection),
// bounding box picking, instanced geometry, and water.  Wraps MasterRenderer.
// Applies view-frustum culling before submitting geometry to MasterRenderer.
//
// Phase 2 Step 3 — Pure Systems:
//   Entity/scene/light lists are no longer passed as constructor arguments.
//   RenderSystem queries the entt::registry directly via views each frame.
//   ChunkManager* (optional) limits visible entities to the active streaming chunk.

#ifndef ENGINE_RENDERSYSTEM_H
#define ENGINE_RENDERSYSTEM_H

#include "ISystem.h"
#include "../Culling/FrustumCuller.h"
#include <entt/entt.hpp>
#include <vector>
#include <glm/glm.hpp>

class MasterRenderer;
class FrameBuffers;
class Terrain;
class Camera;
class InstancedModel;
class ChunkManager;

class RenderSystem : public ISystem {
public:
    /// @param registry        The engine-level ECS registry (source of truth).
    /// @param allTerrains     Reference to the engine's terrain list (updated by StreamingSystem).
    /// @param chunkManager    Optional; if present, active entity/scene subsets are
    ///                        fetched via ChunkManager rather than the full registry.
    RenderSystem(MasterRenderer*        renderer,
                 FrameBuffers*          reflectFbo,
                 entt::registry&        registry,
                 std::vector<Terrain*>& allTerrains,
                 Camera*                camera,
                 const glm::mat4&       projectionMatrix,
                 InstancedModel*        instancedModel = nullptr,
                 ChunkManager*          chunkManager   = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    MasterRenderer*        renderer_;
    FrameBuffers*          reflectFbo_;
    entt::registry&        registry_;
    std::vector<Terrain*>& allTerrains_;
    Camera*                camera_;
    glm::mat4              projectionMatrix_;
    FrustumCuller          culler_;
    InstancedModel*        instancedModel_;
    ChunkManager*          chunkManager_;
};

#endif // ENGINE_RENDERSYSTEM_H

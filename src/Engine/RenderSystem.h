// src/Engine/RenderSystem.h
// Subsystem that owns the primary rendering pass: shadow map, scene, FBO (reflection),
// bounding box picking, instanced geometry, and water.  Wraps MasterRenderer.
// Applies view-frustum culling before submitting geometry to MasterRenderer.

#ifndef ENGINE_RENDERSYSTEM_H
#define ENGINE_RENDERSYSTEM_H

#include "ISystem.h"
#include "../Culling/FrustumCuller.h"
#include <vector>
#include <glm/glm.hpp>

class MasterRenderer;
class FrameBuffers;
class Entity;
class AssimpEntity;
class Terrain;
class Light;
class Interactive;
class Camera;
class InstancedModel;

class RenderSystem : public ISystem {
public:
    RenderSystem(MasterRenderer*           renderer,
                 FrameBuffers*             reflectFbo,
                 std::vector<Entity*>&     entities,
                 std::vector<AssimpEntity*>& scenes,
                 std::vector<Terrain*>&    terrains,
                 std::vector<Light*>&      lights,
                 std::vector<Interactive*>& allBoxes,
                 Camera*                   camera,
                 const glm::mat4&          projectionMatrix,
                 InstancedModel*           instancedModel = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    MasterRenderer*            renderer_;
    FrameBuffers*              reflectFbo_;
    std::vector<Entity*>&      entities_;
    std::vector<AssimpEntity*>& scenes_;
    std::vector<Terrain*>&     terrains_;
    std::vector<Light*>&       lights_;
    std::vector<Interactive*>& allBoxes_;

    Camera*         camera_;
    glm::mat4       projectionMatrix_;
    FrustumCuller   culler_;
    InstancedModel* instancedModel_;
};

#endif // ENGINE_RENDERSYSTEM_H

// src/Engine/RenderSystem.h
// Subsystem that owns the primary rendering pass: shadow map, scene, FBO (reflection),
// bounding box picking, instanced geometry, and water.  Wraps MasterRenderer.
// Applies view-frustum culling before submitting geometry to MasterRenderer.

#ifndef ENGINE_RENDERSYSTEM_H
#define ENGINE_RENDERSYSTEM_H

#include "ISystem.h"
#include "EditorState.h"
#include "TileGridRenderer.h"
#include "../Culling/FrustumCuller.h"
#include <vector>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

class MasterRenderer;
class FrameBuffers;
class Entity;
class Terrain;
class Light;
class Camera;
class Player;
class InstancedModelManager;

class RenderSystem : public ISystem {
public:
    RenderSystem(MasterRenderer*           renderer,
                 FrameBuffers*             reflectFbo,
                 Player*                   player,
                 std::vector<Terrain*>&    terrains,
                 std::vector<Light*>&      lights,
                 entt::registry&           registry,
                 Camera*                   camera,
                 const glm::mat4&          projectionMatrix,
                 InstancedModelManager*    instancedModelMgr = nullptr,
                 EditorState*              editorState       = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    MasterRenderer*            renderer_;
    FrameBuffers*              reflectFbo_;
    Player*                    player_;
    std::vector<Terrain*>&     terrains_;
    std::vector<Light*>&       lights_;
    entt::registry&            registry_;

    Camera*                camera_;
    glm::mat4              projectionMatrix_;
    FrustumCuller          culler_;
    InstancedModelManager* instancedModelMgr_;
    EditorState*           editorState_;
    TileGridRenderer       tileGridRenderer_;
};

#endif // ENGINE_RENDERSYSTEM_H

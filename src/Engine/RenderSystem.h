// src/Engine/RenderSystem.h
// Subsystem that owns the primary rendering pass: scene, FBO (reflection),
// bounding box picking, and water.  Wraps MasterRenderer.

#ifndef ENGINE_RENDERSYSTEM_H
#define ENGINE_RENDERSYSTEM_H

#include "ISystem.h"
#include <vector>

class MasterRenderer;
class FrameBuffers;
class Entity;
class AssimpEntity;
class Terrain;
class Light;
class Interactive;

class RenderSystem : public ISystem {
public:
    RenderSystem(MasterRenderer*           renderer,
                 FrameBuffers*             reflectFbo,
                 std::vector<Entity*>&     entities,
                 std::vector<AssimpEntity*>& scenes,
                 std::vector<Terrain*>&    terrains,
                 std::vector<Light*>&      lights,
                 std::vector<Interactive*>& allBoxes);

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
};

#endif // ENGINE_RENDERSYSTEM_H

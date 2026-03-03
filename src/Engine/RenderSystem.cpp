// src/Engine/RenderSystem.cpp

#include "RenderSystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../RenderEngine/FrameBuffers.h"

RenderSystem::RenderSystem(MasterRenderer*            renderer,
                            FrameBuffers*              reflectFbo,
                            std::vector<Entity*>&      entities,
                            std::vector<AssimpEntity*>& scenes,
                            std::vector<Terrain*>&     terrains,
                            std::vector<Light*>&       lights,
                            std::vector<Interactive*>& allBoxes)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , entities_(entities)
    , scenes_(scenes)
    , terrains_(terrains)
    , lights_(lights)
    , allBoxes_(allBoxes)
{}

void RenderSystem::update(float /*deltaTime*/) {
    // Render bounding boxes into the reflection FBO
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxes(allBoxes_);
    reflectFbo_->unbindCurrentFrameBuffer();

    // Main scene render
    renderer_->renderScene(entities_, scenes_, terrains_, lights_);
}

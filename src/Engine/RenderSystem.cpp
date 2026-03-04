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
                            std::vector<Interactive*>& allBoxes,
                            Camera*                    camera,
                            const glm::mat4&           projectionMatrix)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , entities_(entities)
    , scenes_(scenes)
    , terrains_(terrains)
    , lights_(lights)
    , allBoxes_(allBoxes)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
{}

void RenderSystem::update(float /*deltaTime*/) {
    // Update frustum planes from the current camera position.
    if (camera_) {
        culler_.update(camera_, projectionMatrix_);
    }

    // Cull entities, assimp scenes, and terrain tiles to only visible subsets.
    auto visibleEntities  = camera_ ? culler_.cull(entities_)            : entities_;
    auto visibleScenes    = camera_ ? culler_.cull(scenes_)              : scenes_;
    auto visibleTerrains  = camera_ ? culler_.cullTerrains(terrains_)    : terrains_;

    // Render bounding boxes into the reflection FBO
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxes(allBoxes_);
    reflectFbo_->unbindCurrentFrameBuffer();

    // Main scene render (culled lists)
    renderer_->renderScene(visibleEntities, visibleScenes, visibleTerrains, lights_);
}


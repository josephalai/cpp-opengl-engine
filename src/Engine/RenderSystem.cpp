// src/Engine/RenderSystem.cpp
//
// Phase 2 Step 3 note — Future EnTT migration:
//   Currently RenderSystem iterates the legacy entity vectors (entities_,
//   scenes_, etc.).  A future Phase 3 step will replace these with:
//     registry.view<TransformComponent, RenderComponent>()
//   to batch draw calls over contiguous ECS memory pools.  The legacy
//   entity vectors will be removed once all renderer paths are ported.

#include "RenderSystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../RenderEngine/InstancedModel.h"

RenderSystem::RenderSystem(MasterRenderer*            renderer,
                            FrameBuffers*              reflectFbo,
                            std::vector<Entity*>&      entities,
                            std::vector<AssimpEntity*>& scenes,
                            std::vector<Terrain*>&     terrains,
                            std::vector<Light*>&       lights,
                            std::vector<Interactive*>& allBoxes,
                            Camera*                    camera,
                            const glm::mat4&           projectionMatrix,
                            InstancedModel*            instancedModel)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , entities_(entities)
    , scenes_(scenes)
    , terrains_(terrains)
    , lights_(lights)
    , allBoxes_(allBoxes)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
    , instancedModel_(instancedModel)
{}

void RenderSystem::update(float /*deltaTime*/) {
    // --- Shadow pass — must run before the main scene render ---
    renderer_->renderShadowPass(entities_, lights_);

    // Update frustum planes from the current camera position.
    if (camera_) {
        culler_.update(camera_, projectionMatrix_);
    }

    // Cull entities, assimp scenes, and terrain tiles to only visible subsets.
    auto visibleEntities  = camera_ ? culler_.cull(entities_)         : entities_;
    auto visibleScenes    = camera_ ? culler_.cull(scenes_)           : scenes_;
    auto visibleTerrains  = camera_ ? culler_.cullTerrains(terrains_) : terrains_;

    // Render bounding boxes into the reflection FBO
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxes(allBoxes_);
    reflectFbo_->unbindCurrentFrameBuffer();

    // Main scene render (culled lists)
    renderer_->renderScene(visibleEntities, visibleScenes, visibleTerrains, lights_);

    // Instanced rendering (e.g. 500 trees in one draw call)
    if (instancedModel_ && instancedModel_->getInstanceCount() > 0) {
        renderer_->processInstancedEntity(instancedModel_, instancedModel_->getInstances());
        renderer_->renderInstanced(lights_);
    }

    // Water rendering — must run after opaque geometry
    renderer_->renderWater(camera_, lights_.empty() ? nullptr : lights_[0]);
}


// src/Engine/RenderSystem.cpp

#include "RenderSystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../RenderEngine/InstancedModel.h"
#include "../ECS/Components/AssimpModelComponent.h"

RenderSystem::RenderSystem(MasterRenderer*            renderer,
                            FrameBuffers*              reflectFbo,
                            std::vector<Entity*>&      entities,
                            std::vector<Terrain*>&     terrains,
                            std::vector<Light*>&       lights,
                            entt::registry&            registry,
                            Camera*                    camera,
                            const glm::mat4&           projectionMatrix,
                            InstancedModel*            instancedModel)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , entities_(entities)
    , terrains_(terrains)
    , lights_(lights)
    , registry_(registry)
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

    // Collect Assimp model components from the registry.
    std::vector<AssimpModelComponent> allScenes;
    auto assimpView = registry_.view<AssimpModelComponent>();
    allScenes.reserve(assimpView.size_hint());
    for (auto e : assimpView) {
        allScenes.push_back(assimpView.get<AssimpModelComponent>(e));
    }

    // Cull entities, assimp scenes, and terrain tiles to only visible subsets.
    auto visibleEntities  = camera_ ? culler_.cull(entities_)         : entities_;
    auto visibleScenes    = camera_ ? culler_.cull(allScenes)         : allScenes;
    auto visibleTerrains  = camera_ ? culler_.cullTerrains(terrains_) : terrains_;

    // Render bounding boxes for entities that have them into the reflection FBO
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxes(entities_);
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


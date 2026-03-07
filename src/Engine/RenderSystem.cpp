// src/Engine/RenderSystem.cpp
//
// Phase 2 Step 3 — Mandate 3 + 4: Pure ECS rendering via ActiveChunkTag.
//
// All render lists are built exclusively from registry.view<> queries:
//   - registry.view<EntityOwnerComponent, ActiveChunkTag>()    → Entity* list
//   - registry.view<AssimpModelComponent, ActiveChunkTag>()   → AssimpModelComponent list
//   - registry.view<TerrainComponent,     ActiveChunkTag>()   → Terrain* list
//   - registry.view<LightComponent>()                          → Light* list (always all)
//
// StreamingSystem stamps ActiveChunkTag each frame to control which entities
// are visible.  When no StreamingSystem is present, all entities are permanently
// tagged in Engine::buildSystems().

#include "RenderSystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../RenderEngine/InstancedModel.h"
#include "../ECS/Components/EntityOwnerComponent.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/LightComponent.h"
#include "../ECS/Components/TerrainComponent.h"
#include "../ECS/Components/ActiveChunkTag.h"
#include "../Entities/Entity.h"
#include "../Entities/Light.h"
#include "../Terrain/Terrain.h"
#include "../Interfaces/Interactive.h"

RenderSystem::RenderSystem(MasterRenderer*  renderer,
                            FrameBuffers*    reflectFbo,
                            entt::registry&  registry,
                            Camera*          camera,
                            const glm::mat4& projectionMatrix,
                            InstancedModel*  instancedModel)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , registry_(registry)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
    , instancedModel_(instancedModel)
{}

void RenderSystem::update(float /*deltaTime*/) {
    // -----------------------------------------------------------------------
    // Build entity list — only entities tagged with ActiveChunkTag.
    // -----------------------------------------------------------------------
    std::vector<Entity*> entities;
    {
        auto view = registry_.view<EntityOwnerComponent, ActiveChunkTag>();
        for (auto [e, eoc] : view.each()) {
            if (eoc.ptr) entities.push_back(eoc.ptr);
        }
    }

    // -----------------------------------------------------------------------
    // Build Assimp scene list — only those tagged with ActiveChunkTag.
    // -----------------------------------------------------------------------
    std::vector<AssimpModelComponent> scenes;
    {
        auto view = registry_.view<AssimpModelComponent, ActiveChunkTag>();
        for (auto [e, am] : view.each()) {
            scenes.push_back(am);
        }
    }

    // -----------------------------------------------------------------------
    // Build terrain list — only those tagged with ActiveChunkTag.
    // -----------------------------------------------------------------------
    std::vector<Terrain*> terrains;
    {
        auto view = registry_.view<TerrainComponent, ActiveChunkTag>();
        for (auto [e, tc] : view.each()) {
            if (tc.terrain) terrains.push_back(tc.terrain);
        }
    }

    // -----------------------------------------------------------------------
    // Build lights list — lights are always rendered (no chunk culling for lights).
    // Pointers are stable within this frame since the registry is not modified.
    // -----------------------------------------------------------------------
    std::vector<Light*> lights;
    {
        auto view = registry_.view<LightComponent>();
        for (auto [e, lc] : view.each()) {
            lights.push_back(&lc.light);
        }
    }

    // -----------------------------------------------------------------------
    // Build allBoxes (pickable Interactive* objects) from the active Entity sets.
    // Note: AssimpModelComponent objects are not Interactive and are not pickable.
    // -----------------------------------------------------------------------
    std::vector<Interactive*> allBoxes;
    {
        for (auto* e : entities) {
            if (e && e->getBoundingBox()) allBoxes.push_back(e);
        }
    }

    // --- Shadow pass — must run before the main scene render ---
    renderer_->renderShadowPass(entities, lights);

    // Update frustum planes from the current camera position.
    if (camera_) {
        culler_.update(camera_, projectionMatrix_);
    }

    // Cull to only frustum-visible subsets.
    auto visibleEntities  = camera_ ? culler_.cull(entities)         : entities;
    auto visibleScenes    = camera_ ? culler_.cull(scenes)           : scenes;
    auto visibleTerrains  = camera_ ? culler_.cullTerrains(terrains) : terrains;

    // Render bounding boxes into the reflection FBO.
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxes(allBoxes);
    reflectFbo_->unbindCurrentFrameBuffer();

    // Main scene render (culled lists).
    renderer_->renderScene(visibleEntities, visibleScenes, visibleTerrains, lights);

    // Instanced rendering (e.g. 500 trees in one draw call).
    if (instancedModel_ && instancedModel_->getInstanceCount() > 0) {
        renderer_->processInstancedEntity(instancedModel_, instancedModel_->getInstances());
        renderer_->renderInstanced(lights);
    }

    // Water rendering — must run after opaque geometry.
    renderer_->renderWater(camera_, lights.empty() ? nullptr : lights[0]);
}


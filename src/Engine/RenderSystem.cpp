// src/Engine/RenderSystem.cpp
//
// Phase 2 Step 3 — Pure Systems.
//   Entity, scene, light, and allBoxes lists are discovered each frame via
//   registry.view<> queries instead of legacy side-vectors.
//   If a ChunkManager is present, visible entities/scenes are fetched from
//   the active streaming chunks; otherwise the full registry is iterated.

#include "RenderSystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../RenderEngine/InstancedModel.h"
#include "../ECS/Components/EntityOwnerComponent.h"
#include "../ECS/Components/AssimpComponent.h"
#include "../ECS/Components/LightComponent.h"
#include "../Streaming/ChunkManager.h"
#include "../Entities/Entity.h"
#include "../Entities/AssimpEntity.h"
#include "../Entities/Light.h"
#include "../Interfaces/Interactive.h"

RenderSystem::RenderSystem(MasterRenderer*        renderer,
                            FrameBuffers*          reflectFbo,
                            entt::registry&        registry,
                            std::vector<Terrain*>& allTerrains,
                            Camera*                camera,
                            const glm::mat4&       projectionMatrix,
                            InstancedModel*        instancedModel,
                            ChunkManager*          chunkManager)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , registry_(registry)
    , allTerrains_(allTerrains)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
    , instancedModel_(instancedModel)
    , chunkManager_(chunkManager)
{}

void RenderSystem::update(float /*deltaTime*/) {
    // -----------------------------------------------------------------------
    // Build entity list from registry (filtered by active streaming chunks
    // when a ChunkManager is present).
    // -----------------------------------------------------------------------
    std::vector<Entity*> entities;
    if (chunkManager_) {
        entities = chunkManager_->getActiveEntities();
    } else {
        auto view = registry_.view<EntityOwnerComponent>();
        for (auto [e, eoc] : view.each()) {
            if (eoc.ptr) entities.push_back(eoc.ptr);
        }
    }

    // -----------------------------------------------------------------------
    // Build scene (Assimp) list from registry.
    // -----------------------------------------------------------------------
    std::vector<AssimpEntity*> scenes;
    if (chunkManager_) {
        scenes = chunkManager_->getActiveAssimpEntities();
    } else {
        auto view = registry_.view<AssimpComponent>();
        for (auto [e, ac] : view.each()) {
            if (ac.entity) scenes.push_back(ac.entity);
        }
    }

    // -----------------------------------------------------------------------
    // Build terrain list (from streaming or the fallback allTerrains ref).
    // -----------------------------------------------------------------------
    std::vector<Terrain*> terrains;
    if (chunkManager_) {
        terrains = chunkManager_->getActiveTerrains();
    } else {
        terrains = allTerrains_;
    }

    // -----------------------------------------------------------------------
    // Build lights list from registry.
    // -----------------------------------------------------------------------
    std::vector<Light*> lights;
    {
        auto view = registry_.view<LightComponent>();
        for (auto [e, lc] : view.each()) {
            if (lc.light) lights.push_back(lc.light);
        }
    }

    // -----------------------------------------------------------------------
    // Build allBoxes (pickable Interactive* objects) from registry.
    // -----------------------------------------------------------------------
    std::vector<Interactive*> allBoxes;
    {
        // Entity* objects that have a non-null bounding box
        for (auto* e : entities) {
            if (e && e->getBoundingBox()) allBoxes.push_back(e);
        }
        // AssimpEntity* objects that have a non-null bounding box
        for (auto* s : scenes) {
            if (s && s->getBoundingBox()) allBoxes.push_back(s);
        }
    }

    // --- Shadow pass — must run before the main scene render ---
    renderer_->renderShadowPass(entities, lights);

    // Update frustum planes from the current camera position.
    if (camera_) {
        culler_.update(camera_, projectionMatrix_);
    }

    // Cull entities, assimp scenes, and terrain tiles to only visible subsets.
    auto visibleEntities  = camera_ ? culler_.cull(entities)         : entities;
    auto visibleScenes    = camera_ ? culler_.cull(scenes)           : scenes;
    auto visibleTerrains  = camera_ ? culler_.cullTerrains(terrains) : terrains;

    // Render bounding boxes into the reflection FBO
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxes(allBoxes);
    reflectFbo_->unbindCurrentFrameBuffer();

    // Main scene render (culled lists)
    renderer_->renderScene(visibleEntities, visibleScenes, visibleTerrains, lights);

    // Instanced rendering (e.g. 500 trees in one draw call)
    if (instancedModel_ && instancedModel_->getInstanceCount() > 0) {
        renderer_->processInstancedEntity(instancedModel_, instancedModel_->getInstances());
        renderer_->renderInstanced(lights);
    }

    // Water rendering — must run after opaque geometry
    renderer_->renderWater(camera_, lights.empty() ? nullptr : lights[0]);
}


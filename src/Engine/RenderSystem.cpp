// src/Engine/RenderSystem.cpp

#include "RenderSystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../RenderEngine/InstancedModelManager.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/EditorPlacedComponent.h"
#include "../ECS/Components/LODComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../Entities/Camera.h"
#include "../Toolbox/Maths.h"
#include <cmath>
#include <iostream>

RenderSystem::RenderSystem(MasterRenderer*            renderer,
                            FrameBuffers*              reflectFbo,
                            std::vector<Entity*>&      entities,
                            std::vector<Terrain*>&     terrains,
                            std::vector<Light*>&       lights,
                            entt::registry&            registry,
                            Camera*                    camera,
                            const glm::mat4&           projectionMatrix,
                            InstancedModelManager*     instancedModelMgr,
                            EditorState*               editorState)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , entities_(entities)
    , terrains_(terrains)
    , lights_(lights)
    , registry_(registry)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
    , instancedModelMgr_(instancedModelMgr)
    , editorState_(editorState)
{}

void RenderSystem::update(float /*deltaTime*/) {
    // --- Shadow pass — must run before the main scene render ---
    renderer_->renderShadowPass(entities_, lights_);

    // Update frustum planes from the current camera position.
    if (camera_) {
        culler_.update(camera_, projectionMatrix_);
    }

    // Collect Assimp model components from the registry.
    // Phase 4 Step 4.3 — Apply LOD selection based on camera distance.
    // Entities with a LODComponent get their mesh pointer swapped to the
    // appropriate LOD level's mesh.
    std::vector<AssimpModelComponent> allScenes;
    auto assimpView = registry_.view<AssimpModelComponent>();
    for (auto e : assimpView) {
        auto comp = assimpView.get<AssimpModelComponent>(e);
        if (camera_) {
            auto* lod = registry_.try_get<LODComponent>(e);
            if (lod) {
                float dist = glm::length(camera_->getPosition() - comp.position);
                if (dist < lod->lodDistance0) {
                    lod->currentLOD = 0;
                    // mesh stays as LOD0 (default)
                } else if (dist < lod->lodDistance1) {
                    lod->currentLOD = 1;
                    if (comp.meshLOD1) comp.mesh = comp.meshLOD1;
                } else {
                    lod->currentLOD = 2;
                    if (comp.meshLOD2) comp.mesh = comp.meshLOD2;
                }
            }
        }
        allScenes.push_back(comp);
    }

    // Phase 4 Step 4.1 — Pre-filter entities by SpatialGrid cell visibility.
    // Group entities by their spatial cell, test the cell AABB against the
    // frustum, and skip all entities in cells that are entirely off-screen.
    std::vector<Entity*> cellFilteredEntities;
    if (camera_) {
        static constexpr float kCellSize = 50.0f; // matches SpatialGrid default
        cellFilteredEntities.reserve(entities_.size());
        for (Entity* ent : entities_) {
            if (!ent) continue;
            const glm::vec3& pos = ent->getPosition();
            int cx = static_cast<int>(std::floor(pos.x / kCellSize));
            int cz = static_cast<int>(std::floor(pos.z / kCellSize));
            if (culler_.isCellVisible(cx, cz, kCellSize)) {
                cellFilteredEntities.push_back(ent);
            }
        }
    } else {
        cellFilteredEntities = entities_;
    }

    // Cull entities, assimp scenes, and terrain tiles to only visible subsets.
    auto visibleEntities  = camera_ ? culler_.cull(cellFilteredEntities) : cellFilteredEntities;
    auto visibleScenes    = camera_ ? culler_.cull(allScenes)            : allScenes;
    auto visibleTerrains  = camera_ ? culler_.cullTerrains(terrains_)    : terrains_;

    // Render bounding boxes for entities that have them into the reflection FBO
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxes(entities_);
    reflectFbo_->unbindCurrentFrameBuffer();

    // Main scene render (culled lists)
    renderer_->renderScene(visibleEntities, visibleScenes, visibleTerrains, lights_);

    // Instanced rendering — data-driven via InstancedModelManager (Phase 5.4)
    if (instancedModelMgr_) {
        // Clear the temporary Editor Chunk from the previous frame so placed
        // entities and the ghost preview don't accumulate over time.
        instancedModelMgr_->removeChunk(-1);

        // Render all Editor-Placed Entities dynamically each frame.
        auto editorView = registry_.view<EditorPlacedComponent, TransformComponent>();
        for (auto e : editorView) {
            const auto& epc = editorView.get<EditorPlacedComponent>(e);
            const auto& tc  = editorView.get<TransformComponent>(e);
            if (instancedModelMgr_->hasAlias(epc.prefabAlias)) {
                glm::mat4 matrix = Maths::createTransformationMatrix(
                    tc.position, tc.rotation, tc.scale);
                instancedModelMgr_->addInstance(epc.prefabAlias, -1, matrix);
            }
        }

        // Render the Ghost Preview Cursor tracking the terrain intersection.
        if (editorState_ && editorState_->isEditorMode && editorState_->hasGhostEntity) {
            if (instancedModelMgr_->hasAlias(editorState_->selectedPrefab)) {
                glm::mat4 ghostMatrix = Maths::createTransformationMatrix(
                    editorState_->ghostPosition,
                    glm::vec3(0.0f, editorState_->ghostRotationY, 0.0f),
                    editorState_->ghostScale);
                instancedModelMgr_->addInstance(editorState_->selectedPrefab, -1, ghostMatrix);
            }
        }

        instancedModelMgr_->submitToRenderer(renderer_);
        renderer_->renderInstanced(lights_);
    }

    // Water rendering — must run after opaque geometry
    renderer_->renderWater(camera_, lights_.empty() ? nullptr : lights_[0]);
}
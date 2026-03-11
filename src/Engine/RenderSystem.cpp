// src/Engine/RenderSystem.cpp

#include "RenderSystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../RenderEngine/FrameBuffers.h"
#include "../RenderEngine/InstancedModelManager.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/EditorPlacedComponent.h"
#include "../ECS/Components/LODComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/StaticModelComponent.h"
#include "../Entities/Camera.h"
#include "../Entities/Player.h"
#include "../Toolbox/Maths.h"
#include <cmath>
#include <iostream>

RenderSystem::RenderSystem(MasterRenderer*            renderer,
                            FrameBuffers*              reflectFbo,
                            Player*                    player,
                            std::vector<Terrain*>&     terrains,
                            std::vector<Light*>&       lights,
                            entt::registry&            registry,
                            Camera*                    camera,
                            const glm::mat4&           projectionMatrix,
                            InstancedModelManager*     instancedModelMgr,
                            EditorState*               editorState)
    : renderer_(renderer)
    , reflectFbo_(reflectFbo)
    , player_(player)
    , terrains_(terrains)
    , lights_(lights)
    , registry_(registry)
    , camera_(camera)
    , projectionMatrix_(projectionMatrix)
    , instancedModelMgr_(instancedModelMgr)
    , editorState_(editorState)
{}

void RenderSystem::update(float /*deltaTime*/) {
    // --- Shadow pass — uses registry + Player ---
    renderer_->renderShadowPassFromRegistry(registry_, player_, lights_);

    // Update frustum planes from the current camera position.
    if (camera_) {
        culler_.update(camera_, projectionMatrix_);
    }

    // Collect Assimp model components from the registry.
    // Phase 4 Step 4.3 — Apply LOD selection based on camera distance.
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

    // Cull assimp scenes and terrain tiles to only visible subsets.
    auto visibleScenes    = camera_ ? culler_.cull(allScenes)         : allScenes;
    auto visibleTerrains  = camera_ ? culler_.cullTerrains(terrains_) : terrains_;

    // Render Player bounding box into the reflection FBO for picking.
    reflectFbo_->bindReflectionFrameBuffer();
    renderer_->renderBoundingBoxesFromRegistry(registry_, player_);
    reflectFbo_->unbindCurrentFrameBuffer();

    // Main scene render: Player + StaticModelComponent ECS entities + Assimp
    renderer_->renderSceneFromRegistry(registry_, player_, visibleScenes, visibleTerrains, lights_);

    // Instanced rendering — data-driven via InstancedModelManager (Phase 5.4)
    if (instancedModelMgr_) {
        instancedModelMgr_->removeChunk(-1);

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

// src/Engine/EditorSystem.cpp

#include "EditorSystem.h"
#include "EditorSerializer.h"
#include "../ECS/Components/EditorPlacedComponent.h"
#include "../ECS/Components/InputQueueComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../Config/PrefabManager.h"
#include "../Config/EntityFactory.h"
#include "../Input/InputMaster.h"
#include "../Toolbox/TerrainPicker.h"
#include "../Physics/PhysicsSystem.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>

// ---------------------------------------------------------------------------

EditorSystem::EditorSystem(entt::registry&    registry,
                           TerrainPicker*     picker,
                           PhysicsSystem*     physicsSystem,
                           MasterRenderer*    renderer,
                           EditorState&       editorState,
                           const std::string& sceneJsonPath)
    : registry_(registry)
    , picker_(picker)
    , physicsSystem_(physicsSystem)
    , renderer_(renderer)
    , editorState_(editorState)
    , sceneJsonPath_(sceneJsonPath)
{}

// ---------------------------------------------------------------------------

void EditorSystem::update(float /*deltaTime*/) {
    // --- Populate prefab ID list once ---
    if (prefabIds_.empty()) {
        prefabIds_ = PrefabManager::get().allIds();
        std::sort(prefabIds_.begin(), prefabIds_.end());
    }

    // --- Start ImGui frame ---
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (editorState_.isEditorMode) {
        handleGhostPreview();
        handlePlacement();
        handleEntityDeletion();
        renderEditorWindow();
    }

    // --- Flush ImGui draw data ---
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ---------------------------------------------------------------------------
// Ghost preview — update position to current terrain intersection point.
// ---------------------------------------------------------------------------
void EditorSystem::handleGhostPreview() {
    if (editorState_.selectedPrefab.empty()) {
        editorState_.hasGhostEntity   = false;
        editorState_.placementBlocked = false;
        return;
    }

    if (picker_) {
        glm::vec3 pt = picker_->getCurrentTerrainPoint();
        if (pt != glm::vec3(0.0f)) {
            // --- Compute footprint before snapping (snap is AABB-aware) ---
            glm::vec2 fp = ghostFootprint();
            editorState_.ghostHalfExtents = fp;

            // --- Tile snapping (aligns footprint edges to tile boundaries) ---
            if (editorState_.snapToGrid) {
                pt = TileGrid::snapToGrid(pt, editorState_.tileSize, fp.x, fp.y);
            }

            editorState_.ghostPosition  = pt;
            editorState_.hasGhostEntity = true;

            // --- Overlap check ---
            editorState_.placementBlocked = !TileGrid::isPlacementValid(
                registry_,
                editorState_.ghostPosition,
                fp.x, fp.y);
        } else {
            editorState_.hasGhostEntity   = false;
            editorState_.placementBlocked = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Object placement — spawn entity on left-click (rising edge).
// ---------------------------------------------------------------------------
void EditorSystem::handlePlacement() {
    // Don't place while ImGui is capturing the mouse.
    if (ImGui::GetIO().WantCaptureMouse) {
        prevLeftClick_ = InputMaster::isMouseDown(LeftClick);
        return;
    }

    bool leftNow = InputMaster::isMouseDown(LeftClick);
    bool risingEdge = leftNow && !prevLeftClick_;
    prevLeftClick_ = leftNow;

    if (!risingEdge) return;
    if (editorState_.selectedPrefab.empty()) return;
    if (!editorState_.hasGhostEntity) return;

    // --- Tile overlap guard ---
    if (editorState_.placementBlocked) {
        std::cout << "[Editor] Placement blocked — AABB would overlap an existing entity.\n";
        return;
    }

    glm::vec3 pos = editorState_.ghostPosition;
    // Pass nullptr for physics — editor-placed entities are pure static data
    // containers and must not receive a physics body or character controller.
    entt::entity ent = EntityFactory::spawn(
        registry_, editorState_.selectedPrefab, pos, nullptr);

    if (ent == entt::null) return;

    // Strip input/movement components that EntityFactory adds by default.
    // Static world-props placed via the editor should never be driven by the
    // input or physics systems.
    registry_.remove<InputStateComponent>(ent);
    registry_.remove<InputQueueComponent>(ent);

    // Apply ghost scale and rotation.
    if (auto* tc = registry_.try_get<TransformComponent>(ent)) {
        tc->scale    = editorState_.ghostScale;
        tc->rotation = glm::vec3(0.0f, editorState_.ghostRotationY, 0.0f);
    }

    // Tag as editor-placed.
    registry_.emplace<EditorPlacedComponent>(
        ent,
        editorState_.selectedPrefab,
        editorState_.ghostRotationY);
}

// ---------------------------------------------------------------------------
// Entity deletion — Delete key removes the selected entity.
// ---------------------------------------------------------------------------
void EditorSystem::handleEntityDeletion() {
    if (editorState_.selectedEntity == entt::null) return;
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (InputMaster::isKeyDown(Delete) || InputMaster::isKeyDown(Backspace)) {
        if (registry_.valid(editorState_.selectedEntity)) {
            registry_.destroy(editorState_.selectedEntity);
        }
        editorState_.selectedEntity = entt::null;
    }
}

// ---------------------------------------------------------------------------
// Main editor ImGui window.
// ---------------------------------------------------------------------------
void EditorSystem::renderEditorWindow() {
    ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("World Editor", nullptr, ImGuiWindowFlags_NoCollapse);

    // --- Prefab selection ---
    ImGui::SeparatorText("Prefab");

    if (!prefabIds_.empty()) {
        const char* previewLabel = selectedPrefabIndex_ >= 0
            ? prefabIds_[static_cast<size_t>(selectedPrefabIndex_)].c_str()
            : "(none)";

        if (ImGui::BeginCombo("Prefab##combo", previewLabel)) {
            // Option to clear selection.
            bool noneSelected = (selectedPrefabIndex_ < 0);
            if (ImGui::Selectable("(none)", noneSelected)) {
                selectedPrefabIndex_       = -1;
                editorState_.selectedPrefab.clear();
                editorState_.hasGhostEntity = false;
            }
            for (int i = 0; i < static_cast<int>(prefabIds_.size()); ++i) {
                bool selected = (i == selectedPrefabIndex_);
                if (ImGui::Selectable(prefabIds_[static_cast<size_t>(i)].c_str(), selected)) {
                    selectedPrefabIndex_       = i;
                    editorState_.selectedPrefab = prefabIds_[static_cast<size_t>(i)];
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled("No prefabs loaded");
    }

    // --- Ghost controls ---
    ImGui::SeparatorText("Ghost Controls");
    ImGui::SliderFloat("Scale##ghost",    &editorState_.ghostScale,     0.1f, 10.0f);
    ImGui::SliderFloat("Rotation Y##ghost", &editorState_.ghostRotationY, 0.0f, 360.0f);

    // --- Tile Grid ---
    ImGui::SeparatorText("Tile Grid (~ shows grid)");
    ImGui::Checkbox("Snap to Grid",  &editorState_.snapToGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Show Grid",     &editorState_.showTileGrid);
    ImGui::SliderFloat("Tile Size##tile", &editorState_.tileSize, 1.0f, 32.0f, "%.1f m");

    // --- Placement info ---
    ImGui::SeparatorText("Placement Info");
    if (editorState_.hasGhostEntity) {
        ImGui::Text("Terrain hit: (%.1f, %.1f, %.1f)",
            editorState_.ghostPosition.x,
            editorState_.ghostPosition.y,
            editorState_.ghostPosition.z);
        if (editorState_.placementBlocked) {
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "BLOCKED — entity would overlap");
        } else {
            ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "OK — tile is free");
        }
    } else {
        ImGui::TextDisabled("No terrain intersection");
    }

    // Count editor-placed entities.
    int placedCount = 0;
    {
        auto v = registry_.view<EditorPlacedComponent>();
        for (auto e : v) { (void)e; ++placedCount; }
    }
    ImGui::Text("Placed entities: %d", placedCount);

    ImGui::Spacing();
    ImGui::TextWrapped("Left-click on terrain to place the selected prefab.");

    // --- Entity list ---
    renderEntityList();

    // --- Transform editor ---
    renderTransformEditor();

    // --- Save / Load ---
    ImGui::SeparatorText("Map");
    if (ImGui::Button("Save Map", ImVec2(-1, 0))) {
        EditorSerializer::saveToJson(sceneJsonPath_, registry_);
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// List of all editor-placed entities.
// ---------------------------------------------------------------------------
void EditorSystem::renderEntityList() {
    ImGui::SeparatorText("Entity List");

    auto view = registry_.view<EditorPlacedComponent, TransformComponent>();

    ImGui::BeginChild("##entity_list", ImVec2(0, 120), true);
    for (auto entity : view) {
        const auto& epc = view.get<EditorPlacedComponent>(entity);
        bool selected = (entity == editorState_.selectedEntity);

        // Use entt entity ID as a unique label suffix.
        char label[128];
        std::snprintf(label, sizeof(label), "%s##ent%u",
            epc.prefabAlias.c_str(),
            static_cast<unsigned>(entt::to_integral(entity)));

        if (ImGui::Selectable(label, selected)) {
            editorState_.selectedEntity = entity;
        }
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Transform editor for the selected entity.
// ---------------------------------------------------------------------------
void EditorSystem::renderTransformEditor() {
    if (editorState_.selectedEntity == entt::null) return;
    if (!registry_.valid(editorState_.selectedEntity)) {
        editorState_.selectedEntity = entt::null;
        return;
    }

    auto* tc  = registry_.try_get<TransformComponent>(editorState_.selectedEntity);
    auto* epc = registry_.try_get<EditorPlacedComponent>(editorState_.selectedEntity);
    if (!tc || !epc) return;

    ImGui::SeparatorText("Transform");
    ImGui::DragFloat3("Position##sel", &tc->position.x, 0.1f);
    ImGui::SliderFloat("Rotation Y##sel", &epc->rotationY, 0.0f, 360.0f);
    tc->rotation.y = epc->rotationY;
    ImGui::SliderFloat("Scale##sel", &tc->scale, 0.1f, 10.0f);

    if (ImGui::Button("Delete Entity", ImVec2(-1, 0))) {
        registry_.destroy(editorState_.selectedEntity);
        editorState_.selectedEntity = entt::null;
    }
}

// ---------------------------------------------------------------------------
// ghostFootprint — XZ half-extents of the selected prefab × ghostScale.
// ---------------------------------------------------------------------------
glm::vec2 EditorSystem::ghostFootprint() const {
    if (editorState_.selectedPrefab.empty()) {
        return { editorState_.ghostScale * 0.5f,
                 editorState_.ghostScale * 0.5f };
    }

    const auto& j = PrefabManager::get().getPrefab(editorState_.selectedPrefab);
    if (!j.is_null() && j.contains("physics") &&
        j["physics"].contains("halfExtents")) {
        const auto& he = j["physics"]["halfExtents"];
        if (he.is_array() && he.size() >= 3) {
            float hx = he[0].get<float>() * editorState_.ghostScale;
            float hz = he[2].get<float>() * editorState_.ghostScale;
            return { hx, hz };
        }
    }
    return { editorState_.ghostScale * 0.5f,
             editorState_.ghostScale * 0.5f };
}

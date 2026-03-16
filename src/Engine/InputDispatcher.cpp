// src/Engine/InputDispatcher.cpp

#include "InputDispatcher.h"
#include "../Events/Event.h"
#include "../Events/EventBus.h"
#include "../Events/EntityClickedEvent.h"
#include "../Input/InputMaster.h"
#include "../Toolbox/TerrainPicker.h"
#include "../Entities/Camera.h"
#include "../Entities/CameraInput.h"
#include "../RenderEngine/DisplayManager.h"
#include "../Interaction/EntityPicker.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../Guis/UiMaster.h"
#include "../Guis/ContextMenu/ContextMenu.h"
#include "../Guis/Chat/ChatBox.h"
#include "../Config/PrefabManager.h"

#include <imgui.h>
#include <iostream>

InputDispatcher::InputDispatcher(TerrainPicker*  picker,
                                 EditorState*    editorState,
                                 EntityPicker*   entityPicker,
                                 Camera*         camera,
                                 glm::mat4       projection,
                                 entt::registry* registry)
    : picker_(picker)
    , editorState_(editorState)
    , entityPicker_(entityPicker)
    , camera_(camera)
    , projection_(projection)
    , registry_(registry)
{}

// ---------------------------------------------------------------------------
// tryPickEntity — shared helper for both left-click and right-click.
// Casts a ray from the current cursor position (or screen centre in FPS
// mode) and fires EntityClickedEvent if a NetworkIdComponent entity is hit.
// @return true if an entity was found and the event published.
// ---------------------------------------------------------------------------
bool InputDispatcher::tryPickEntity() {
    if (!entityPicker_ || !camera_ || !registry_) return false;

    // When the cursor is locked (FPS / invisible cursor mode), mouseX/mouseY
    // accumulate as virtual coordinates that can be far outside the screen
    // bounds.  Use the screen centre instead so the player "clicks" where
    // they are looking (crosshair).  In point-and-click mode the real cursor
    // position is used as expected.
    float pickX, pickY;
    if (CameraInput::cursorInvisible) {
        pickX = static_cast<float>(DisplayManager::Width())  * 0.5f;
        pickY = static_cast<float>(DisplayManager::Height()) * 0.5f;
    } else {
        pickX = static_cast<float>(InputMaster::mouseX);
        pickY = static_cast<float>(InputMaster::mouseY);
    }

    glm::vec3 rayOrigin, rayDir;
    EntityPicker::buildPickRay(
        pickX, pickY,
        DisplayManager::Width(),
        DisplayManager::Height(),
        projection_,
        camera_->getViewMatrix(),
        rayOrigin,
        rayDir);

    entt::entity hit = entityPicker_->pick(rayOrigin, rayDir);
    if (hit == entt::null) {
        std::cout << "[Input] Pick ray — no entity hit.\n";
        return false;
    }

    auto* nid = registry_->try_get<NetworkIdComponent>(hit);
    if (!nid) {
        std::cout << "[Input] Pick ray hit entity but it has no NetworkIdComponent.\n";
        return false;
    }

    EntityClickedEvent evt{};
    evt.networkId = nid->id;
    EventBus::instance().publish(evt);
    std::cout << "[Input] Entity clicked — Network ID: " << nid->id
              << "  model=" << nid->modelType << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// Phase 2 — spawnContextMenu
// Reads actions from the entity's prefab JSON and shows the OSRS-style
// right-click context menu at the current cursor position.
// Returns true if the menu was shown (entity with NetworkIdComponent found).
// ---------------------------------------------------------------------------
bool InputDispatcher::spawnContextMenu() {
    if (!entityPicker_ || !camera_ || !registry_) return false;

    float pickX = static_cast<float>(InputMaster::mouseX);
    float pickY = static_cast<float>(InputMaster::mouseY);

    glm::vec3 rayOrigin, rayDir;
    EntityPicker::buildPickRay(
        pickX, pickY,
        DisplayManager::Width(),
        DisplayManager::Height(),
        projection_,
        camera_->getViewMatrix(),
        rayOrigin,
        rayDir);

    entt::entity hit = entityPicker_->pick(rayOrigin, rayDir);
    if (hit == entt::null) return false;

    auto* nid = registry_->try_get<NetworkIdComponent>(hit);
    if (!nid) return false;

    // Retrieve actions from the prefab definition.
    std::vector<ContextMenuAction> actions;
    if (PrefabManager::get().hasPrefab(nid->modelType)) {
        const auto& prefab = PrefabManager::get().getPrefab(nid->modelType);
        if (prefab.contains("actions") && prefab["actions"].is_array()) {
            for (const auto& a : prefab["actions"]) {
                ContextMenuAction act;
                act.id    = a.value("id",    0);
                act.label = a.value("label", "Examine");
                actions.push_back(act);
            }
        }
    }
    // Always provide a fallback "Examine" option.
    if (actions.empty()) {
        actions.push_back({0, "Examine"});
    }

    ContextMenu::instance().show(pickX, pickY, nid->id, nid->modelType, actions);
    std::cout << "[Input] Context menu for entity " << nid->id
              << " (" << nid->modelType << ")\n";
    return true;
}

void InputDispatcher::update(float /*deltaTime*/) {
    // --- ImGui input multiplexing ---
    // When ImGui has captured the keyboard, skip all gameplay key dispatch.
    // When ImGui has captured the mouse, skip all gameplay mouse dispatch.
    const bool imguiWantsKeyboard = ImGui::GetIO().WantCaptureKeyboard;
    const bool imguiWantsMouse    = ImGui::GetIO().WantCaptureMouse;

    // --- Tilde (~) press: toggle editor mode (rising-edge, not suppressed by ImGui) ---
    if (editorState_) {
        bool tildeNow = InputMaster::isKeyDown(GraveAccent);
        if (tildeNow && !editorState_->prevTildeDown) {
            editorState_->isEditorMode = !editorState_->isEditorMode;

            if (editorState_->isEditorMode) {
                // Entering editor: save camera state, show cursor, enable god mode.
                editorState_->savedCameraPos   = Camera::Position;
                editorState_->savedCameraYaw   = Camera::Yaw;
                editorState_->savedCameraPitch = Camera::Pitch;
                Camera::godMode = true;
                glfwSetInputMode(DisplayManager::window,
                                 GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                // Exiting editor: restore camera state, hide cursor, disable god mode.
                Camera::Position = editorState_->savedCameraPos;
                Camera::Yaw      = editorState_->savedCameraYaw;
                Camera::Pitch    = editorState_->savedCameraPitch;
                Camera::godMode  = false;
                // Restore invisible cursor if it was invisible before.
                if (CameraInput::cursorInvisible) {
                    glfwSetInputMode(DisplayManager::window,
                                     GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                DisplayManager::resetMouse = true;
            }
        }
        editorState_->prevTildeDown = tildeNow;
    }

    // While in editor mode, skip all gameplay input dispatch.
    if (editorState_ && editorState_->isEditorMode) {
        prevRightClick_ = InputMaster::isMouseDown(RightClick);
        prevLeftClick_  = InputMaster::isMouseDown(LeftClick);
        return;
    }

    // -----------------------------------------------------------------------
    // Phase 1: Suppress WASD while the chat input field is focused.
    // -----------------------------------------------------------------------
    const bool chatTyping = ChatBox::instance().isTyping();

    // --- Skip gameplay input when ImGui has keyboard focus or chat is active ---
    if (!imguiWantsKeyboard && !chatTyping) {
        // --- Movement command ---
        PlayerMoveCommandEvent moveCmd{};
        moveCmd.forward     = InputMaster::isActionDown("MoveForward")  ?  1.0f
                            : InputMaster::isActionDown("MoveBackward") ? -1.0f : 0.0f;
        // A/D are strafe keys in camera-relative movement.
        // +1 = strafe left (A), -1 = strafe right (D).
        moveCmd.strafe      = InputMaster::isActionDown("MoveLeft")     ?  1.0f
                            : InputMaster::isActionDown("MoveRight")    ? -1.0f : 0.0f;
        // Camera::Yaw is kept in sync with PlayerCamera::orbitYaw_ each frame
        // by PlayerCamera::calculateCameraPosition so we can read it here.
        moveCmd.cameraYaw   = Camera::Yaw;
        moveCmd.jump        = InputMaster::isActionDown("Jump");
        moveCmd.sprint      = InputMaster::isActionDown("Sprint");
        moveCmd.sprintReset = InputMaster::isActionDown("SprintReset");

        EventBus::instance().publish(moveCmd);
    }

    if (!imguiWantsMouse) {
        // -----------------------------------------------------------------------
        // Phase 1: UI input consumption — skip world interaction when the cursor
        // is over a UI panel or the context menu is active.
        // -----------------------------------------------------------------------
        const bool uiConsumed = UiMaster::isMouseOverUi();

        // -------------------------------------------------------------------
        // Right-click (rising edge)
        // -------------------------------------------------------------------
        bool rightNow = InputMaster::isMouseDown(RightClick);
        if (rightNow && !prevRightClick_) {
            if (!uiConsumed) {
                // Phase 2: Attempt to show the OSRS context menu for the
                // hovered entity.  If no entity is under the cursor, fall back
                // to the terrain walk destination.
                bool menuShown = spawnContextMenu();
                if (!menuShown && picker_) {
                    glm::vec3 pt = picker_->getCurrentTerrainPoint();
                    if (pt != glm::vec3(0.0f)) {
                        TargetLocationClickedEvent evt{};
                        evt.worldPosition = pt;
                        EventBus::instance().publish(evt);
                    }
                }
            }
        }
        prevRightClick_ = rightNow;

        // -------------------------------------------------------------------
        // Left-click (rising edge): dismiss context menu first; if the menu
        // was not open, attempt an entity pick (no terrain fallback).
        // -------------------------------------------------------------------
        bool leftNow = InputMaster::isMouseDown(LeftClick);
        if (leftNow && !prevLeftClick_) {
            if (ContextMenu::instance().isVisible()) {
                // ContextMenu::render() handles dismissal on outside-click via
                // ImGui — no extra action needed here.
            } else if (!uiConsumed) {
                tryPickEntity();
            }
        }
        prevLeftClick_ = leftNow;
    } else {
        prevRightClick_ = InputMaster::isMouseDown(RightClick);
        prevLeftClick_  = InputMaster::isMouseDown(LeftClick);
    }
}

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

#include <imgui.h>

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
        return;
    }

    // --- Skip gameplay input when ImGui has focus ---
    if (!imguiWantsKeyboard) {
        // --- Movement command ---
        PlayerMoveCommandEvent moveCmd{};
        moveCmd.forward     = InputMaster::isActionDown("MoveForward")  ?  1.0f
                            : InputMaster::isActionDown("MoveBackward") ? -1.0f : 0.0f;
        moveCmd.turn        = InputMaster::isActionDown("MoveLeft")     ?  1.0f
                            : InputMaster::isActionDown("MoveRight")    ? -1.0f : 0.0f;
        moveCmd.jump        = InputMaster::isActionDown("Jump");
        moveCmd.sprint      = InputMaster::isActionDown("Sprint");
        moveCmd.sprintReset = InputMaster::isActionDown("SprintReset");

        EventBus::instance().publish(moveCmd);
    }

    if (!imguiWantsMouse) {
        // --- Right-click (rising edge) ---
        // Priority: Entity picking → Terrain fallback.
        bool rightNow = InputMaster::isMouseDown(RightClick);
        if (rightNow && !prevRightClick_) {

            bool entityHit = false;

            // 1. Try entity picking first (requires EntityPicker + Camera).
            if (entityPicker_ && camera_ && registry_) {
                glm::vec3 rayOrigin, rayDir;
                EntityPicker::buildPickRay(
                    static_cast<float>(InputMaster::mouseX),
                    static_cast<float>(InputMaster::mouseY),
                    DisplayManager::Width(),
                    DisplayManager::Height(),
                    projection_,
                    camera_->getViewMatrix(),
                    rayOrigin,
                    rayDir);

                entt::entity hit = entityPicker_->pick(rayOrigin, rayDir);
                if (hit != entt::null) {
                    if (auto* nid = registry_->try_get<NetworkIdComponent>(hit)) {
                        EntityClickedEvent evt{};
                        evt.networkId = nid->id;
                        EventBus::instance().publish(evt);
                        entityHit = true;
                    }
                }
            }

            // 2. Fallback to terrain walk if no entity was hit.
            if (!entityHit && picker_) {
                glm::vec3 pt = picker_->getCurrentTerrainPoint();
                if (pt != glm::vec3(0.0f)) {
                    TargetLocationClickedEvent evt{};
                    evt.worldPosition = pt;
                    EventBus::instance().publish(evt);
                }
            }
        }
        prevRightClick_ = rightNow;
    } else {
        prevRightClick_ = InputMaster::isMouseDown(RightClick);
    }
}

// src/Engine/InputSystem.cpp

#include "InputSystem.h"
#include "../Entities/PlayerCamera.h"
#include "../Entities/Camera.h"
#include "../Entities/CameraInput.h"
#include "../Terrain/Terrain.h"
#include "../Toolbox/TerrainPicker.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Guis/Text/GUIText.h"
#include "../Input/InputMaster.h"
#include "../RenderEngine/DisplayManager.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

InputSystem::InputSystem(PlayerCamera* camera,
                         Terrain*       primaryTerrain,
                         TerrainPicker* picker,
                         GuiTexture*    sampleGui,
                         GUIText*       pNameText,
                         EditorState*   editorState)
    : camera_(camera)
    , terrain_(primaryTerrain)
    , picker_(picker)
    , sampleGui_(sampleGui)
    , pNameText_(pNameText)
    , editorState_(editorState)
{}

void InputSystem::update(float deltaTime) {
    if (editorState_ && editorState_->isEditorMode) {
        // God-mode: free-fly camera, update picker for ghost preview.
        updateGodCamera(deltaTime);
        if (picker_) picker_->update();
    } else {
        // Normal mode: orbit camera + picker.
        if (camera_)  camera_->move(terrain_);
        if (picker_)  picker_->update();

        // Simple per-frame GUI animations
        if (sampleGui_) sampleGui_->getPosition() += glm::vec2(0.001f, 0.001f);
        if (pNameText_) pNameText_->getPosition() += glm::vec2(0.1f);
    }
}

// ---------------------------------------------------------------------------
// God-mode free-fly camera.
// WASD to fly, Shift to sprint.  Right-mouse + drag to rotate view.
// ---------------------------------------------------------------------------
void InputSystem::updateGodCamera(float deltaTime) {
    // Don't move the camera when ImGui is consuming keyboard input.
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    // Speed (hold Shift to sprint).
    float speed = CameraInput::kSpeed * 3.0f * deltaTime;
    if (InputMaster::isKeyDown(LeftShift) || InputMaster::isKeyDown(RightShift)) {
        speed *= 4.0f;
    }

    // WASD movement along Camera's Front / Right vectors.
    if (InputMaster::isKeyDown(W)) {
        Camera::Position += Camera::Front * speed;
    }
    if (InputMaster::isKeyDown(S)) {
        Camera::Position -= Camera::Front * speed;
    }
    if (InputMaster::isKeyDown(A)) {
        Camera::Position -= Camera::Right * speed;
    }
    if (InputMaster::isKeyDown(D)) {
        Camera::Position += Camera::Right * speed;
    }
    // E / C for vertical movement.
    if (InputMaster::isKeyDown(E)) {
        Camera::Position += Camera::Up * speed;
    }
    if (InputMaster::isKeyDown(C)) {
        Camera::Position -= Camera::Up * speed;
    }

    // Right-mouse drag → look around.
    if (InputMaster::isMouseDown(RightClick)) {
        float dx = InputMaster::mouseDx * CameraInput::kSensitivity;
        float dy = InputMaster::mouseDy * CameraInput::kSensitivity;
        Camera::Yaw   += dx;
        Camera::Pitch += dy;
        if (Camera::Pitch >  89.0f) Camera::Pitch =  89.0f;
        if (Camera::Pitch < -89.0f) Camera::Pitch = -89.0f;
    }
    // Reset mouse delta after we've consumed it.
    InputMaster::mouseDx = 0.0f;
    InputMaster::mouseDy = 0.0f;

    // Rebuild Camera vectors from updated Yaw/Pitch.
    // (PlayerCamera::updateCameraVectors() is orbit-specific, so we inline the
    // standard free-fly trig here instead of calling it through the pointer.)
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Camera::Yaw)) * cos(glm::radians(Camera::Pitch));
        front.y = sin(glm::radians(Camera::Pitch));
        front.z = sin(glm::radians(Camera::Yaw)) * cos(glm::radians(Camera::Pitch));
        Camera::Front = glm::normalize(front);
        Camera::Right = glm::normalize(glm::cross(Camera::Front, Camera::WorldUp));
        Camera::Up    = glm::normalize(glm::cross(Camera::Right, Camera::Front));
    }
}

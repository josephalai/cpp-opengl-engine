//
// Created by Joseph Alai on 7/18/21.
//
#include "PlayerCamera.h"
#include "../Config/ConfigManager.h"
#include "../RenderEngine/DisplayManager.h"
#include <cmath>
#include <algorithm>

/**
 * @brief move (MAIN LOOP), modifies the actual camera vectors based on the
 *        modification of the Player's vectors. This includes Zooming, rotating,
 *        and movement.
 * @param terrain
 */
void PlayerCamera::move(Terrain *terrain) {
    const float deltaTime = DisplayManager::getFrameTimeSeconds();

    this->processInput(DisplayManager::window);
    DisplayManager::uniformMovement();
    updateCameraVectors();
    calculateZoom();
    calculateAngleAroundPlayer();
    player->move(terrain);

    // Initialise orbit yaw and pivot on the first frame so the camera
    // starts directly behind the player.
    const glm::vec3 playerPosition = player->getPosition();
    if (!pivotInitialized_) {
        orbitYaw_         = player->getRotation().y;
        orbitYawInitialized_ = true;
        pivotPosition_    = playerPosition;
        pivotInitialized_ = true;
    } else {
        // Smoothly advance the orbit pivot toward the player's actual position.
        // This prevents the camera from rigidly snapping with the player during
        // server-authoritative auto-walk (reconcile LERP) while remaining
        // imperceptibly tight during normal keyboard-driven movement.
        const float alpha = 1.0f - std::exp(-kPivotSmoothing * deltaTime);
        pivotPosition_    = glm::mix(pivotPosition_, playerPosition, alpha);
    }

    float horizontalDistance = calculateHorizontalDistance();
    float verticalDistance = calculateVerticalDistance();
    calculateCameraPosition(horizontalDistance, verticalDistance);
}

void PlayerCamera::calculateCameraPosition(float horizDistance, float verticDistance) const {
    // Use the camera's own orbit yaw directly — it never reads player->getRotation().y.
    // This is the core of camera-relative movement: the camera angle is fully
    // independent of which direction the player model is facing.
    const float theta = orbitYaw_;

    float offsetX = horizDistance * sin(glm::radians(theta));
    float offsetZ = horizDistance * cos(glm::radians(theta));
    // Orbit around the smoothed pivot, not the raw player position, so the
    // camera glides rather than snapping rigidly during auto-walk LERP steps.
    Position.x = pivotPosition_.x - offsetX;
    Position.z = pivotPosition_.z - offsetZ;
    Position.y = pivotPosition_.y - verticDistance + kOrbitPivotY;

    // Keep Camera::Yaw equal to orbitYaw_ so that InputDispatcher,
    // PlayerMovementSystem, and NetworkSystem can read the camera direction
    // via the globally accessible Camera::Yaw without a direct PlayerCamera ptr.
    Yaw = orbitYaw_;
}

// returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 PlayerCamera::getViewMatrix() {
    // In god (editor) mode, bypass the orbit camera and use a standard free-fly
    // view so the developer can navigate independently of the player.  Camera::Front
    // has already been updated by InputSystem::updateGodCamera() this frame.
    if (Camera::godMode) {
        return glm::lookAt(Camera::Position, Camera::Position + Camera::Front, Camera::Up);
    }
    return glm::lookAt(PlayerCamera::Position, pivotPosition_ + glm::vec3(0, kOrbitPivotY, 0), PlayerCamera::Up);
}

void PlayerCamera::calculateAngleAroundPlayer() {
    if (glfwGetMouseButton(DisplayManager::window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS || cursorInvisible) {
        float angleChange = InputMaster::mouseDx *
            ConfigManager::get().client.camera.mouseSensitivity;
        // orbitYaw_ is the single source of truth for the camera's world angle.
        // Mouse drag rotates it directly — no rate-limiter, no detach/attach logic.
        orbitYaw_ -= angleChange;
    }
}

float PlayerCamera::calculateHorizontalDistance() const {
    return (float) (distanceFromPlayer * cos(glm::radians(Pitch +
        ConfigManager::get().client.camera.pitchOffset)));
}

float PlayerCamera::calculateVerticalDistance() const {
    return (float) (distanceFromPlayer * sin(glm::radians(Pitch +
        ConfigManager::get().client.camera.pitchOffset)));
}

void PlayerCamera::updateCameraVectors() {
    Front = glm::vec3(0, 0, 1);
    Up = glm::vec3(0, 1, 0);
    Right = glm::vec3(1, 0, 0);
}

void PlayerCamera::calculateZoom() {
    const auto& camCfg = ConfigManager::get().client.camera;
    float zoomLevel = ZoomOffset * camCfg.zoomSensitivity;
    distanceFromPlayer += zoomLevel;
    if (distanceFromPlayer < camCfg.minZoomDistance) {
        distanceFromPlayer = camCfg.minZoomDistance;
    }
    if (distanceFromPlayer > camCfg.maxZoomDistance) {
        distanceFromPlayer = camCfg.maxZoomDistance;
    }
}

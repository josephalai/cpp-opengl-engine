//
// Created by Joseph Alai on 7/18/21.
//
#include "PlayerCamera.h"

/**
 * @brief move (MAIN LOOP), modifies the actual camera vectors based on the
 *        modification of the Player's vectors. This includes Zooming, rotating,
 *        and movement.
 * @param terrain
 */
void PlayerCamera::move(Terrain *terrain) {
    this->processInput(DisplayManager::window);
    DisplayManager::uniformMovement();
    updateCameraVectors();
    calculateZoom();
    calculateAngleAroundPlayer();
    player->move(terrain);
    float horizontalDistance = calculateHorizontalDistance();
    float verticalDistance = calculateVerticalDistance();
    calculateCameraPosition(horizontalDistance, verticalDistance);
}

void PlayerCamera::calculateCameraPosition(float horizDistance, float verticDistance) const {
    float theta = player->getRotation().y + angleAroundPlayer;
    float offsetX = horizDistance * sin(glm::radians(theta));
    float offsetZ = horizDistance * cos(glm::radians(theta));
    Position.x = player->getPosition().x - offsetX;
    Position.z = player->getPosition().z - offsetZ;
    Position.y = player->getPosition().y - verticDistance + kOrbitPivotY;

    if (glfwGetMouseButton(DisplayManager::window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS || cursorInvisible) {
        Yaw = static_cast<int>(180 - player->getRotation().y + angleAroundPlayer - 90) % 360;
    }
}

// returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 PlayerCamera::getViewMatrix() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    return glm::lookAt(PlayerCamera::Position, player->getPosition() + glm::vec3(0, kOrbitPivotY, 0), PlayerCamera::Up);
}

void PlayerCamera::calculateAngleAroundPlayer() {
    if (glfwGetMouseButton(DisplayManager::window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS || cursorInvisible) {
        float angleChange = InputMaster::mouseDx * 0.1f;
        angleAroundPlayer -= angleChange;
    }
}

float PlayerCamera::calculateHorizontalDistance() const {
    return (float) (distanceFromPlayer * cos(glm::radians(Pitch + 4)));
}

float PlayerCamera::calculateVerticalDistance() const {
    return (float) (distanceFromPlayer * sin(glm::radians(Pitch + 4)));
}

void PlayerCamera::updateCameraVectors() {
    Front = glm::vec3(0, 0, 1);
    Up = glm::vec3(0, 1, 0);
    Right = glm::vec3(1, 0, 0);
}

void PlayerCamera::calculateZoom() {
    float zoomLevel = ZoomOffset * 0.03f;
    distanceFromPlayer += zoomLevel;
    if (distanceFromPlayer < 5) {
        distanceFromPlayer = 5;
    }
}
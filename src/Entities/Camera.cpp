//
// Created by Joseph Alai on 7/6/21.
//
#include "Camera.h"

glm::vec3 Camera::Position;
glm::vec3 Camera::Front;
glm::vec3 Camera::Up;
glm::vec3 Camera::Right;
glm::vec3 Camera::WorldUp;
// euler Angles
float Camera::Yaw = 90.0f;
float Camera::Pitch;
bool  Camera::godMode = false;

/**
 * @brief A constructor with vectors... An abstract camera class
 *        that processes input and calculates the corresponding
 *        Euler Angles, Vectors and Matrices for use in OpenGL.
 *
 */
Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) {
    Front = (glm::vec3(0.0f, 0.0f, -1.0f));
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

/**
 * @brief updates the sight vector
 */
void Camera::move() {
    updateCameraVectors();
}

/**
 * @brief returns the view matrix calculated using Euler Angles and the LookAt Matrix
 * @return
 */
glm::mat4 Camera::getViewMatrix() {
    return glm::lookAt(Position, Position + Front, Up);
}

/**
 * @brief calculates the Right and Up vectors to be used by the glm::lookAt function,
 *        based on the Pitch and Yaw (established by the CameraInput or PlayerCamera.
 */
void Camera::updateCameraVectors() {
    // calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    // also re-calculate the Right and Up vector
    Right = glm::normalize(glm::cross(Front,
                                      WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    Up = glm::normalize(glm::cross(Right, Front));
}
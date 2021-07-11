//
// Created by Joseph Alai on 7/6/21.
//
#include "Camera.h"

// constructor with vectors
Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
    this->Position = position;
    this->WorldUp = up;
    this->Yaw = yaw;
    this->Pitch = pitch;
    this->updateCameraVectors();
}

// constructor with scalar values
Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
    this->Position = glm::vec3(posX, posY, posZ);
    this->WorldUp = glm::vec3(upX, upY, upZ);
    this->Yaw = yaw;
    this->Pitch = pitch;
    this->updateCameraVectors();
}

// returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 Camera::GetViewMatrix() {
    return glm::lookAt(this->Position, this->Position + this->Front, this->Up);
}

void Camera::toggleFirstPersonShooter(bool enabled) {
    this->fps = enabled;
}

// processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    float velocity = this->MovementSpeed * deltaTime;
    if (direction == FORWARD)
        this->Position += this->Front * velocity;
    if (direction == BACKWARD)
        this->Position -= this->Front * velocity;
    if (direction == LEFT)
        this->Position -= this->Right * velocity;
    if (direction == RIGHT)
        this->Position += this->Right * velocity;
    if (this->fps) {
        this->Position.y = 3;
    }

}

// processes input received from a mouse input system. Expects the offset value in both the x and y direction.
void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= this->MouseSensitivity;
    yoffset *= this->MouseSensitivity;

    this->Yaw   += xoffset;
    this->Pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
        if (this->Pitch > 89.0f)
            this->Pitch = 89.0f;
        if (this->Pitch < -89.0f)
            this->Pitch = -89.0f;
    }

    // update Front, Right and Up Vectors using the updated Euler angles
    this->updateCameraVectors();
}

// processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
void Camera::ProcessMouseScroll(float yoffset) {
    this->Zoom += (float)yoffset;
    if (this->Zoom < MIN_ZOOM)
        this->Zoom = MIN_ZOOM;
    if (this->Zoom > MAX_ZOOM)
        this->Zoom = MAX_ZOOM;
}

void Camera::updateCameraVectors() {
    // calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(this->Yaw)) * cos(glm::radians(this->Pitch));
    front.y = sin(glm::radians(this->Pitch));
    front.z = sin(glm::radians(this->Yaw)) * cos(glm::radians(this->Pitch));
    this->Front = glm::normalize(front);
    // also re-calculate the Right and Up vector
    this->Right = glm::normalize(glm::cross(this->Front, this->WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    this->Up    = glm::normalize(glm::cross(this->Right, this->Front));
}
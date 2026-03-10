#ifndef CAMERA_H
#define CAMERA_H
#define GL_SILENCE_DEPRECATION

#include "GLFW/glfw3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include "Player.h"

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;

/**
 * @brief An abstract camera class that processes input and calculates the corresponding
 *        Euler Angles, Vectors and Matrices for use in OpenGL
 */
class Camera {
public:
    // camera Attributes
    static glm::vec3 Position;
    static glm::vec3 Front;
    static glm::vec3 Up;
    static glm::vec3 Right;
    static glm::vec3 WorldUp;
    // euler Angles
    static float Yaw;
    static float Pitch;

    // ADD THIS GETTER:
    glm::vec3 getPosition() const { return Position; }

    /**
     * @brief A constructor with vectors... An abstract camera class
     *        that processes input and calculates the corresponding
     *        Euler Angles, Vectors and Matrices for use in OpenGL.
     *
     */
    explicit Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
                    float yaw = YAW, float pitch = PITCH);

    // updates the sight vector;
    virtual void move();

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    virtual glm::mat4 getViewMatrix();

protected:

    // calculates the front vector from the Camera's (updated) Euler Angles
    static void updateCameraVectors();
};

#endif

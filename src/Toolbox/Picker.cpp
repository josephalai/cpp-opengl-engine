//
// Created by Joseph Alai on 7/26/21.
//

#include "Picker.h"
#include "../Entities/CameraInput.h"
#include "GLFW/glfw3.h"

glm::vec3 Picker::calculateMouseRay(glm::mat4 projectionMatrix, glm::mat4 viewMatrix) {
    glm::vec2 normalizedCoords = getNormalizedDeviceCoords();
    glm::vec4 clipCoords = glm::vec4(normalizedCoords.x, normalizedCoords.y, -1.0f, 1.0f);
    glm::vec4 eyeCoords = toEyeCoords(clipCoords, projectionMatrix );
    glm::vec3 worldCoords = toWorldCoords(eyeCoords, viewMatrix);
    return worldCoords;
}

glm::vec3 Picker::toWorldCoords(glm::vec4 eyeCoords, glm::mat4 viewMatrix) {
    glm::vec3 rayWorld = glm::inverse(viewMatrix) * eyeCoords;
    return glm::normalize(rayWorld);
}

glm::vec4 Picker::toEyeCoords(glm::vec4 clipCoords, glm::mat4 projectionMatrix) {
    glm::vec4 invertedProjection = glm::inverse(projectionMatrix) * clipCoords;
    return glm::vec4(invertedProjection.x, invertedProjection.y, -1.0f, 0.0f);
}

glm::vec2 Picker::getNormalizedDeviceCoords() {
    float x = (2.0f * static_cast<float>(InputMaster::mouseX)) / static_cast<float>(DisplayManager::Width()) - 1;
    float y = (2.0f * static_cast<float>(InputMaster::mouseY)) / static_cast<float>(DisplayManager::Height()) - 1;
    return glm::vec2(x, -y);
}

Color Picker::getColor() {
    unsigned char pixel[4];
    glFlush();
    glFinish();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    int width, height;
    glfwGetFramebufferSize(DisplayManager::window, &width, &height);
    glReadPixels(static_cast<int>(width / DisplayManager::Width() * static_cast<int>(InputMaster::mouseX)),
                 static_cast<int>(static_cast<int>(height) - (height / DisplayManager::Height() * static_cast<int>(InputMaster::mouseY))), 1, 1, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixel);
    return Color(pixel[0], pixel[1], pixel[2], pixel[3]);
}
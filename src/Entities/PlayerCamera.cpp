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

    // Edge-detect ESC to toggle detached rotation (single press, not held).
    // Set escConsumedByDetach_ so our processInput() override skips the
    // cursor-style toggle on the same frame, preventing double-consumption.
    {
        bool escNow = InputMaster::isKeyDown(Escape);
        if (escNow && !prevEscDown_) {
            setDetachedRotation(!detachedRotation_);
            escConsumedByDetach_ = true;
        }
        prevEscDown_ = escNow;
    }

    // Advance the mode-transition blend fraction each frame.
    // transitionFraction_ == 1 → fully attached; 0 → fully detached.
    // The speed is read from client_settings.json ("transition_speed") so
    // it can be tuned without recompiling.
    const float transitionSpeed = ConfigManager::get().client.camera.transitionSpeed;
    if (detachedRotation_) {
        transitionFraction_ = std::max(0.0f, transitionFraction_ - transitionSpeed * deltaTime);
    } else {
        transitionFraction_ = std::min(1.0f, transitionFraction_ + transitionSpeed * deltaTime);
    }

    // Re-anchor worldAngle_ each frame while transitioning back to attached
    // mode.  The shrinking (1 - t) fraction then always measures from where
    // the camera currently IS, preventing overshoot when the player moves.
    if (!detachedRotation_ && transitionFraction_ < 1.0f) {
        worldAngle_ = effectiveOrbitAngle();
    }

    // When the transition fully completes, snap to a clean attached state.
    if (!detachedRotation_ && transitionFraction_ >= 1.0f) {
        transitionFraction_ = 1.0f;
        worldAngle_ = player->getRotation().y + angleAroundPlayer;
    }

    this->processInput(DisplayManager::window);
    DisplayManager::uniformMovement();
    updateCameraVectors();
    calculateZoom();
    calculateAngleAroundPlayer();
    player->move(terrain);

    // Smoothly advance the orbit pivot toward the player's actual position.
    // This prevents the camera from rigidly snapping with the player during
    // server-authoritative auto-walk (reconcile LERP) while remaining
    // imperceptibly tight during normal keyboard-driven movement.
    const glm::vec3 playerPosition = player->getPosition();
    if (!pivotInitialized_) {
        pivotPosition_    = playerPosition;
        pivotInitialized_ = true;
    } else {
        const float alpha = 1.0f - std::exp(-kPivotSmoothing * deltaTime);
        pivotPosition_    = glm::mix(pivotPosition_, playerPosition, alpha);
    }

    float horizontalDistance = calculateHorizontalDistance();
    float verticalDistance = calculateVerticalDistance();
    calculateCameraPosition(horizontalDistance, verticalDistance);
}

void PlayerCamera::calculateCameraPosition(float horizDistance, float verticDistance) const {
    // Compute the camera orbit angle as a smooth blend between the fully-
    // attached angle (follows player yaw) and the fully-detached angle
    // (fixed in world space).  transitionFraction_ == 1 → attached,
    // transitionFraction_ == 0 → detached.
    float theta = effectiveOrbitAngle();

    float offsetX = horizDistance * sin(glm::radians(theta));
    float offsetZ = horizDistance * cos(glm::radians(theta));
    // Orbit around the smoothed pivot, not the raw player position, so the
    // camera glides rather than snapping rigidly during auto-walk LERP steps.
    Position.x = pivotPosition_.x - offsetX;
    Position.z = pivotPosition_.z - offsetZ;
    Position.y = pivotPosition_.y - verticDistance + kOrbitPivotY;

    if (glfwGetMouseButton(DisplayManager::window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS || cursorInvisible) {
        // Keep Camera::Yaw in sync with the effective orbit angle so tools
        // like terrain pickers and god-mode restore have a consistent value.
        // Use the double-modulo form to guarantee a non-negative result in [0, 360).
        Yaw = ((static_cast<int>(90 - theta) % 360) + 360) % 360;
    }
}

// returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 PlayerCamera::getViewMatrix() {
    // In god (editor) mode, bypass the orbit camera and use a standard free-fly
    // view so the developer can navigate independently of the player.  Camera::Front
    // has already been updated by InputSystem::updateGodCamera() this frame.
    if (Camera::godMode) {
        return glm::lookAt(Camera::Position, Camera::Position + Camera::Front, Camera::Up);
    }
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    return glm::lookAt(PlayerCamera::Position, pivotPosition_ + glm::vec3(0, kOrbitPivotY, 0), PlayerCamera::Up);
}

void PlayerCamera::calculateAngleAroundPlayer() {
    if (glfwGetMouseButton(DisplayManager::window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS || cursorInvisible) {
        float angleChange = InputMaster::mouseDx *
            ConfigManager::get().client.camera.mouseSensitivity;
        if (detachedRotation_) {
            // Detached: orbit around the fixed world angle — only worldAngle_
            // advances, player->getRotation().y is not involved.
            worldAngle_ -= angleChange;
        } else {
            // Attached: orbit relative to the player's facing direction.
            angleAroundPlayer -= angleChange;
        }
    }

    // In attached mode, clamp the orbit angle so the camera can never wrap
    // around to face the player from the front.
    if (!detachedRotation_) {
        angleAroundPlayer = clampOrbitAngle(angleAroundPlayer);
    }
}

// ---------------------------------------------------------------------------
// setDetachedRotation
// ---------------------------------------------------------------------------

void PlayerCamera::setDetachedRotation(bool detach) {
    if (detach == detachedRotation_) return;

    // Compute the current effective orbit angle from the blend state so
    // that toggling mid-transition never causes a visible position jump.
    float currentTheta = effectiveOrbitAngle();

    if (detach) {
        // Entering detached: anchor worldAngle_ at the current effective
        // position.  transitionFraction_ will now decrease toward 0.
        // Normalize to prevent unbounded drift over time.
        worldAngle_ = wrapAngle(currentTheta);
    } else {
        // Returning to attached: adjust angleAroundPlayer so that the
        // attached formula evaluates to currentTheta right now, and
        // update worldAngle_ to the same value so the lerp has no jump.
        // Normalize both angles to prevent drift, then clamp so the
        // re-attached camera always starts from behind the player.
        angleAroundPlayer = clampOrbitAngle(wrapAngle(currentTheta - player->getRotation().y));
        worldAngle_ = wrapAngle(currentTheta);
    }

    detachedRotation_ = detach;
}

// ---------------------------------------------------------------------------
// effectiveOrbitAngle — private helper
// ---------------------------------------------------------------------------

// Wraps an angle difference to [-180, +180) so that interpolation always
// takes the shortest arc rather than the long way around.
float PlayerCamera::wrapAngle(float a) {
    a = fmod(a + 180.0f, 360.0f);
    if (a < 0.0f) a += 360.0f;
    return a - 180.0f;
}

// Clamps an orbit angle to [-maxOrbitAngle, +maxOrbitAngle] as configured
// in client_settings.json so the camera can never face the player head-on.
float PlayerCamera::clampOrbitAngle(float a) {
    const float limit = ConfigManager::get().client.camera.maxOrbitAngle;
    return std::max(-limit, std::min(limit, a));
}

float PlayerCamera::effectiveOrbitAngle() const {
    float thetaAttached = player->getRotation().y + angleAroundPlayer;
    // Use the shortest arc so the camera never sweeps the long way around.
    float delta = wrapAngle(thetaAttached - worldAngle_);
    return worldAngle_ + transitionFraction_ * delta;
}

// ---------------------------------------------------------------------------
// processInput — override to prevent ESC double-consumption
// ---------------------------------------------------------------------------

void PlayerCamera::processInput(GLFWwindow *window) {
    // If the ESC key was consumed by the detach toggle this frame, tell the
    // parent to skip the cursor-style toggle so cursorInvisible doesn't flip
    // to true (which would cause all mouse movement to orbit the camera
    // without any button held down).
    if (escConsumedByDetach_) {
        escConsumedByDetach_ = false;
        skipEscCursorToggle_ = true;
    }
    CameraInput::processInput(window);
}

// ---------------------------------------------------------------------------
// setAutoWalkActive
// ---------------------------------------------------------------------------

void PlayerCamera::setAutoWalkActive(bool active) {
    if (active == autoWalkActive_) return;
    autoWalkActive_ = active;

    if (active) {
        // Save the current mode and enter detached so player facing changes
        // during auto-walk don't swing the camera around.
        autoWalkWasDetached_ = detachedRotation_;
        setDetachedRotation(true);
    } else {
        // Restore the mode that was active before auto-walk, with a smooth
        // transition back (prevents the jagged snap when auto-walk ends).
        setDetachedRotation(autoWalkWasDetached_);
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
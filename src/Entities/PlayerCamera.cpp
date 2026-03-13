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

    // Keep worldAngle_ in sync with the attached orbit angle so that when
    // detached mode is entered, worldAngle_ is already at the correct value.
    if (!detachedRotation_) {
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

    // Rate-limit the rendered orbit angle (currentOrbitAngle_) toward the
    // logical target (effectiveOrbitAngle()).  This is what prevents the
    // camera from "blasting" during automatic mode changes: no matter how
    // abruptly the target changes, the camera only ever moves at most
    // 360° / rotation_360_time degrees per second.
    //
    // Mouse-driven orbit (see calculateAngleAroundPlayer) updates
    // currentOrbitAngle_ directly so there is no lag during manual control.
    {
        const float targetAngle = effectiveOrbitAngle();
        if (!orbitAngleInitialized_) {
            currentOrbitAngle_    = targetAngle;
            orbitAngleInitialized_ = true;
        } else {
            const float degsPerSec = 360.0f /
                std::max(0.01f, ConfigManager::get().client.camera.rotation360Time);
            const float maxDeg = degsPerSec * deltaTime;
            const float diff   = wrapAngle(targetAngle - currentOrbitAngle_);
            if (std::abs(diff) <= maxDeg) {
                currentOrbitAngle_ = targetAngle;
            } else {
                currentOrbitAngle_ += (diff > 0.0f ? maxDeg : -maxDeg);
            }
        }
    }

    float horizontalDistance = calculateHorizontalDistance();
    float verticalDistance = calculateVerticalDistance();
    calculateCameraPosition(horizontalDistance, verticalDistance);
}

void PlayerCamera::calculateCameraPosition(float horizDistance, float verticDistance) const {
    // Use the rate-limited rendered orbit angle so the camera never blasts
    // to a new position — it sweeps there at the configured angular speed.
    const float theta = currentOrbitAngle_;

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
            worldAngle_        -= angleChange;
            currentOrbitAngle_ -= angleChange;  // instant response — no rate-limit for mouse
        } else {
            // Attached: orbit relative to the player's facing direction.
            angleAroundPlayer  -= angleChange;
            currentOrbitAngle_ -= angleChange;  // instant response — no rate-limit for mouse
        }
    }

    // In attached mode, clamp the orbit angle so the camera can never wrap
    // around to face the player from the front.
    if (!detachedRotation_) {
        angleAroundPlayer  = clampOrbitAngle(angleAroundPlayer);
        currentOrbitAngle_ = clampOrbitAngle(currentOrbitAngle_);
    }
}

// ---------------------------------------------------------------------------
// setDetachedRotation
// ---------------------------------------------------------------------------

void PlayerCamera::setDetachedRotation(bool detach) {
    if (detach == detachedRotation_) return;

    // Use the visual camera position (currentOrbitAngle_) as the anchor so
    // that toggling mid-transition never causes a visible jump.  If the
    // camera hasn't started rendering yet, fall back to the logical angle.
    float currentTheta = orbitAngleInitialized_ ? currentOrbitAngle_ : effectiveOrbitAngle();

    if (detach) {
        // Entering detached: anchor worldAngle_ exactly where the camera
        // visually is right now.  Setting transitionFraction_ = 0 immediately
        // prevents the "blast" — even if the player yaw jumps in the same
        // frame (e.g. pathfinding faces the NPC), effectiveOrbitAngle()
        // returns worldAngle_ and currentOrbitAngle_ does not chase it.
        worldAngle_         = wrapAngle(currentTheta);
        transitionFraction_ = 0.0f;
    } else {
        // Returning to attached: compute angleAroundPlayer so that the
        // attached formula evaluates to the current visual angle right now,
        // then immediately snap the logical state to fully attached.
        // currentOrbitAngle_ will smoothly chase the new target (which may
        // differ if the player has rotated during auto-walk).
        angleAroundPlayer   = clampOrbitAngle(wrapAngle(currentTheta - player->getRotation().y));
        worldAngle_         = wrapAngle(currentTheta);
        transitionFraction_ = 1.0f;
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
    // Return the logical target angle for the current mode.
    // Smoothing is handled by currentOrbitAngle_ in move(), not here.
    if (detachedRotation_) {
        return worldAngle_;
    }
    return player->getRotation().y + angleAroundPlayer;
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
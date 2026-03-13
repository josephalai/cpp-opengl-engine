//
// Created by Joseph Alai on 7/11/21.
//

#ifndef ENGINE_PLAYERCAMERA_H
#define ENGINE_PLAYERCAMERA_H

#include "Camera.h"
#include "CameraInput.h"

class PlayerCamera : public CameraInput {
public:
    Player *player;

    float distanceFromPlayer = 55.0f;

    // Vertical offset of the orbit pivot above the player's origin.
    // Must be consistent between calculateCameraPosition() and getViewMatrix().
    // Set to approximately the character's chest height so the camera centers
    // on the body rather than above the head when zoomed in close.
    constexpr static const float kOrbitPivotY = 1.5f;

    /**
     * @brief PlayerCamera (extending CameraInput), is modified based on the player's movements.
     *        This in turn updates vectors and matrices in CameraInput, which then modifies the
     *        vectors and matrices in Camera, which ultimately, later is retrieved by:
     *        getViewMatrix(), loaded into a shader, and rendered on the screen.
     *
     *        When the player modifies the vectors (transformations), by the keyboard and mouse,
     *        the camera actually modifies itself based on those movements. Again, this is all
     *        just manipulation of vectors. Nothing is being rendered yet.
     *
     * @param player
     */
    explicit PlayerCamera(Player *player) : player(player), CameraInput() {
        Pitch = -20.0f;
    }

    void move(Terrain *terrain);

    void calculateCameraPosition(float horizDistance, float verticDistance) const;

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 getViewMatrix() override;

    void calculateAngleAroundPlayer();

    /// Returns the camera's absolute world-space orbit yaw (degrees).
    /// Used by NetworkSystem to send the correct movement direction and by
    /// PlayerMovementSystem for camera-relative strafe.
    /// Camera::Yaw is also kept equal to this value each frame.
    float getOrbitYaw() const { return orbitYaw_; }

private:
    float calculateHorizontalDistance() const;

    float calculateVerticalDistance() const;

    void updateCameraVectors() override;

    void calculateZoom();

    // Smoothed orbit-pivot position.  Updated each frame in move() by
    // exponentially following player->getPosition().  Using this instead
    // of the raw player position removes the "rigid" camera snap during
    // server-authoritative auto-walk (reconcile LERP).
    glm::vec3 pivotPosition_   = glm::vec3(0.0f);
    bool      pivotInitialized_ = false;

    // Spring strength for the camera pivot.  Higher = tighter follow.
    // At 60 fps (dt≈0.016 s): alpha = 1 − exp(−15 × 0.016) ≈ 0.21.
    // Normal walking speed ≈7 u/s → steady-state lag < 0.5 u (imperceptible).
    // Auto-walk LERP jumps of 2–3 u are smoothed over ~150 ms.
    static constexpr float kPivotSmoothing = 15.0f;

    // -----------------------------------------------------------------------
    // Camera-relative movement: single absolute world-space orbit angle.
    //
    // orbitYaw_ is the only rotation state the camera owns.  It is modified
    // exclusively by mouse drag (calculateAngleAroundPlayer).  The player's
    // rotation.y is never read for camera positioning — this prevents the
    // "whip" when auto-walk or pathfinding changes the player's facing.
    //
    // Initialised to player->getRotation().y on the first frame so the camera
    // starts directly behind the player.
    // -----------------------------------------------------------------------
    float orbitYaw_ = 0.0f;
    bool  orbitYawInitialized_ = false;
};

#endif //ENGINE_PLAYERCAMERA_H

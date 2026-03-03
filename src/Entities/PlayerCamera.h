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
    float angleAroundPlayer = 0.0f;

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
     *        When the player modifies the vectors (transformations), by the keyboad and mouse,
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

private:
    float calculateHorizontalDistance() const;

    float calculateVerticalDistance() const;

    void updateCameraVectors() override;

    void calculateZoom();


};

#endif //ENGINE_PLAYERCAMERA_H

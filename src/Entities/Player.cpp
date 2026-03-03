#include "Player.h"
#include "../Input/InputMaster.h"

//
// Created by Joseph Alai on 7/10/21.
//
float Player::SPEED_HACK = 1.0f;

/**
 * @brief move (Main Loop), takes the terrain and moves the character based on the
 *        inputs. It MOVES / ROTATES it on the current terrain. It's utilizing Entity's
 *        information, as well as the DisplayManager to move around.
 * @param terrain
 */
void Player::move(Terrain *terrain) {
    checkInputs();
    rotate(glm::vec3(0.0f, currentTurnSpeed * DisplayManager::getFrameTimeSeconds(), 0.0f));
    float distance = currentSpeed * DisplayManager::getFrameTimeSeconds();
    float dx = distance * sin(glm::radians(getRotation().y));
    float dz = distance * cos(glm::radians(getRotation().y));
    increasePosition(glm::vec3(dx, 0.0f, dz));
    upwardsSpeed += kGravity * DisplayManager::getFrameTimeSeconds();
    increasePosition(glm::vec3(0, upwardsSpeed * DisplayManager::getFrameTimeSeconds(), 0.0f));

    float terrainHeight = terrain->getHeightOfTerrain(getPosition().x, getPosition().z);
    if (getPosition().y <= terrainHeight) {
        upwardsSpeed = 0.0f;
        setPosition(+glm::vec3(getPosition().x, terrainHeight, getPosition().z));
        isInAir = false;
    }
}

void Player::jump() {
    if (!isInAir) {
        upwardsSpeed = kJumpPower;
        isInAir = true;
    }
}

void Player::checkInputs() {

    if (InputMaster::isKeyDown(W)) {
        currentSpeed = kRunSpeed * SPEED_HACK;
    } else if (InputMaster::isKeyDown(S)) {
        currentSpeed = -kRunSpeed * SPEED_HACK;
    } else {
        currentSpeed = 0.0f;
    }

    if (InputMaster::isKeyDown(D)) {
        currentTurnSpeed = -kTurnSpeed * SPEED_HACK / 2;
    } else if (InputMaster::isKeyDown(A)) {
        currentTurnSpeed = kTurnSpeed * SPEED_HACK / 2;
    } else {
        currentTurnSpeed = 0.0f;
    }

    if (InputMaster::isKeyDown(Space)) {
        jump();
    }


    if (InputMaster::isKeyDown(Tab)) {
        SPEED_HACK = 4.5f;
    }
    if (InputMaster::isKeyDown(Backslash)) {
        SPEED_HACK = 1.0f;
    }
}
// src/Engine/InputSystem.cpp

#include "InputSystem.h"
#include "../Entities/Player.h"
#include "../Entities/PlayerCamera.h"
#include "../Entities/Components/InputComponent.h"
#include "../Physics/PhysicsSystem.h"
#include "../Terrain/Terrain.h"
#include "../Toolbox/TerrainPicker.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Guis/Text/GUIText.h"
#include "../Input/InputMaster.h"

#include <glm/glm.hpp>
#include <cmath>

InputSystem::InputSystem(PlayerCamera*  camera,
                         Terrain*       primaryTerrain,
                         TerrainPicker* picker,
                         GuiTexture*    sampleGui,
                         GUIText*       pNameText,
                         Player*        player,
                         PhysicsSystem* physics)
    : camera_(camera)
    , terrain_(primaryTerrain)
    , picker_(picker)
    , sampleGui_(sampleGui)
    , pNameText_(pNameText)
    , player_(player)
    , physicsSystem_(physics)
{}

void InputSystem::update(float deltaTime) {
    // -----------------------------------------------------------------------
    // 1. Apply player input — logic extracted from InputComponent::update().
    //    Reads the public data fields of the attached InputComponent and
    //    applies rotation + physics walk-direction (or legacy terrain movement)
    //    each frame.  The InputComponent's EventBus subscription keeps
    //    currentSpeed_ / currentTurnSpeed_ up-to-date between frames.
    // -----------------------------------------------------------------------
    if (player_) {
        auto* ic = player_->getComponent<InputComponent>();
        if (ic) {
            // If not using EventBus, poll InputMaster directly (legacy path).
            if (!ic->useEventBus_) {
                ic->checkInputs();
            }

            // Apply yaw rotation.
            player_->rotate(glm::vec3(0.0f, ic->currentTurnSpeed_ * deltaTime, 0.0f));

            const float sinY = std::sin(glm::radians(player_->getRotation().y));
            const float cosY = std::cos(glm::radians(player_->getRotation().y));

            if (physicsSystem_) {
                // Bullet physics path: set kinematic character walk direction.
                physicsSystem_->setPlayerWalkDirection(
                    ic->currentSpeed_ * sinY * deltaTime,
                    ic->currentSpeed_ * cosY * deltaTime,
                    InputMaster::isKeyDown(Space));
            } else if (terrain_) {
                // Legacy path: manual gravity + terrain-height collision.
                const float distance = ic->currentSpeed_ * deltaTime;
                player_->increasePosition(glm::vec3(distance * sinY, 0.0f, distance * cosY));

                ic->upwardsSpeed_ += InputComponent::kGravity * deltaTime;
                player_->increasePosition(glm::vec3(0.0f, ic->upwardsSpeed_ * deltaTime, 0.0f));

                const float terrainH = terrain_->getHeightOfTerrain(
                    player_->getPosition().x, player_->getPosition().z);
                if (player_->getPosition().y <= terrainH) {
                    ic->upwardsSpeed_ = 0.0f;
                    player_->setPosition(glm::vec3(
                        player_->getPosition().x, terrainH, player_->getPosition().z));
                    ic->isInAir_ = false;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 2. Camera movement + terrain picker (reads updated player position/rotation).
    // -----------------------------------------------------------------------
    if (camera_)  camera_->move(terrain_);
    if (picker_)  picker_->update();

    // Simple per-frame GUI animations
    if (sampleGui_) sampleGui_->getPosition() += glm::vec2(0.001f, 0.001f);
    if (pNameText_) pNameText_->getPosition() += glm::vec2(0.1f);
}

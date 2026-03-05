#include "Player.h"
#include "Components/InputComponent.h"

//
// Created by Joseph Alai on 7/10/21.
//

Player::Player(TexturedModel *model, BoundingBox *box, glm::vec3 position,
               glm::vec3 rotation, float scale)
    : Entity(model, box, position, rotation, scale)
{
    inputComponent_ = addComponent<InputComponent>();
}

/**
 * @brief move (Main Loop), takes the terrain and moves the character based on the
 *        inputs. It delegates to the InputComponent via updateComponents().
 * @param terrain
 */
void Player::move(Terrain *terrain) {
    if (inputComponent_) {
        inputComponent_->setTerrain(terrain);
    }
    float dt = DisplayManager::getFrameTimeSeconds();
    updateComponents(dt);
}

void Player::setPhysicsSystem(PhysicsSystem* ps) {
    if (inputComponent_) {
        inputComponent_->setPhysicsSystem(ps);
    }
}

void Player::subscribeToEvents() {
    if (inputComponent_) {
        inputComponent_->subscribeToEvents();
    }
}

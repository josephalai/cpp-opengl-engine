#include "Player.h"
#include "Components/InputComponent.h"

//
// Created by Joseph Alai on 7/10/21.
//

Player::Player(entt::registry& registry, TexturedModel *model, BoundingBox *box,
               glm::vec3 position, glm::vec3 rotation, float scale)
    : Entity(registry, model, box, position, rotation, scale)
{
    inputComponent_ = addComponent<InputComponent>();
}

/**
 * @brief move — sets terrain context on the InputComponent so the legacy
 *        terrain-height path has the data it needs.  Per-frame movement
 *        (rotation, physics walk-direction) is now applied by InputSystem.
 * @param terrain
 */
void Player::move(Terrain *terrain) {
    if (inputComponent_) {
        inputComponent_->setTerrain(terrain);
    }
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

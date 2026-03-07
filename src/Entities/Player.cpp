#include "Player.h"

//
// Created by Joseph Alai on 7/10/21.
//

Player::Player(entt::registry& registry, TexturedModel *model, BoundingBox *box,
               glm::vec3 position, glm::vec3 rotation, float scale)
    : Entity(registry, model, box, position, rotation, scale)
{}

void Player::move(Terrain* /*terrain*/) {
    // Movement is handled by PlayerMovementSystem (ECS).
}

void Player::setPhysicsSystem(PhysicsSystem* /*ps*/) {
    // Physics wiring is handled via InputStateComponent (ECS).
}

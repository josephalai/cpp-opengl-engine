//
// CharacterController.cpp
//

#include "CharacterController.h"
#include "PhysicsSystem.h"
#include "../Entities/Player.h"
#include "../Terrain/Terrain.h"
#include "../Input/InputMaster.h"
#include "../RenderEngine/DisplayManager.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

CharacterController::CharacterController(PhysicsSystem* physicsSystem, Player* player,
                                          float capsuleRadius, float capsuleHeight)
    : physicsSystem_(physicsSystem), player_(player) {
    physicsSystem_->setCharacterController(player, capsuleRadius, capsuleHeight);
    controller_ = physicsSystem_->getWorld()
        ? nullptr  // Bullet controller is managed inside PhysicsSystem
        : nullptr;
}

CharacterController::~CharacterController() = default;

void CharacterController::update(float /*deltaTime*/, Terrain* /*terrain*/) {
    // Input is already handled by Player::checkInputs() via PlayerCamera::move().
    // Here we only need to forward the walk direction and jump to Bullet.
    // PhysicsSystem::update() will step the simulation and sync the ghost
    // transform back to player->setPosition() after each frame.
    //
    // Note: Player::move() is NOT called when a CharacterController is active;
    // PlayerCamera::move() should be updated to use this controller instead.
    // For backward compatibility the full integration is wired in PhysicsSystem.
}

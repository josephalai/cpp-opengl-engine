#include "InputComponent.h"
#include "../Entity.h"
#include "../../Physics/PhysicsSystem.h"
#include "../../Events/Event.h"
#include "../../Events/EventBus.h"

#include <glm/glm.hpp>
#include <cmath>
#include <iostream>

float InputComponent::SPEED_HACK = 1.0f;

void InputComponent::init() {
    std::cout << "[InputComponent] Initialized and attached to entity." << std::endl;
}

void InputComponent::update(float deltaTime) {
    checkInputs();

    // Apply yaw rotation
    entity_->rotate(glm::vec3(0.0f, currentTurnSpeed_ * deltaTime, 0.0f));

    float sinY = std::sin(glm::radians(entity_->getRotation().y));
    float cosY = std::cos(glm::radians(entity_->getRotation().y));

    if (physicsSystem_) {
        physicsSystem_->setPlayerWalkDirection(
            currentSpeed_ * sinY * deltaTime,
            currentSpeed_ * cosY * deltaTime,
            InputMaster::isKeyDown(Space));
        return;
    }

    // Legacy path: manual gravity + terrain-height collision.
    float distance = currentSpeed_ * deltaTime;
    entity_->increasePosition(glm::vec3(distance * sinY, 0.0f, distance * cosY));

    upwardsSpeed_ += kGravity * deltaTime;
    entity_->increasePosition(glm::vec3(0.0f, upwardsSpeed_ * deltaTime, 0.0f));

    if (terrain_) {
        float terrainHeight = terrain_->getHeightOfTerrain(
            entity_->getPosition().x, entity_->getPosition().z);
        if (entity_->getPosition().y <= terrainHeight) {
            upwardsSpeed_ = 0.0f;
            entity_->setPosition(glm::vec3(entity_->getPosition().x,
                                           terrainHeight,
                                           entity_->getPosition().z));
            isInAir_ = false;
        }
    }
}

void InputComponent::subscribeToEvents() {
    useEventBus_ = true;
    EventBus::instance().subscribe<PlayerMoveCommandEvent>(
        [this](const PlayerMoveCommandEvent& cmd) {
            applyMovementCommand(cmd.forward, cmd.turn,
                                 cmd.jump, cmd.sprint, cmd.sprintReset);
        });
}

void InputComponent::checkInputs() {
    if (useEventBus_) {
        return; // state already updated by EventBus handler
    }

    float fwd  = InputMaster::isKeyDown(W) ?  1.0f
               : InputMaster::isKeyDown(S) ? -1.0f : 0.0f;
    float turn = InputMaster::isKeyDown(A) ?  1.0f
               : InputMaster::isKeyDown(D) ? -1.0f : 0.0f;
    applyMovementCommand(fwd, turn,
                         InputMaster::isKeyDown(Space),
                         InputMaster::isKeyDown(Tab),
                         InputMaster::isKeyDown(Backslash));
}

void InputComponent::applyMovementCommand(float forward, float turn,
                                          bool jump, bool sprint, bool sprintReset) {
    if      (forward > 0.0f) currentSpeed_ =  kRunSpeed * SPEED_HACK;
    else if (forward < 0.0f) currentSpeed_ = -kRunSpeed * SPEED_HACK;
    else                     currentSpeed_ = 0.0f;

    if      (turn > 0.0f) currentTurnSpeed_ =  kTurnSpeed * SPEED_HACK / 2.0f;
    else if (turn < 0.0f) currentTurnSpeed_ = -kTurnSpeed * SPEED_HACK / 2.0f;
    else                  currentTurnSpeed_ = 0.0f;

    if (sprint)      SPEED_HACK = 4.5f;
    if (sprintReset) SPEED_HACK = 1.0f;

    if (jump) this->jump();
}

void InputComponent::jump() {
    if (!isInAir_) {
        upwardsSpeed_ = kJumpPower;
        isInAir_ = true;
    }
}

#include "InputComponent.h"
#include "../Entity.h"
#include "../../Physics/PhysicsSystem.h"
#include "../../Events/Event.h"
#include "../../Events/EventBus.h"
#include <nlohmann/json.hpp>

#include <glm/glm.hpp>
#include <cmath>
#include <iostream>

float InputComponent::SPEED_HACK = 1.0f;

void InputComponent::init() {
    std::cout << "[InputComponent] Initialized and attached to entity." << std::endl;
}

void InputComponent::initFromJson(const nlohmann::json& j) {
    if (j.contains("run_speed"))  runSpeed_  = j["run_speed"].get<float>();
    if (j.contains("turn_speed")) turnSpeed_ = j["turn_speed"].get<float>();
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
    if      (forward > 0.0f) currentSpeed_ =  runSpeed_ * SPEED_HACK;
    else if (forward < 0.0f) currentSpeed_ = -runSpeed_ * SPEED_HACK;
    else                     currentSpeed_ = 0.0f;

    if      (turn > 0.0f) currentTurnSpeed_ =  turnSpeed_ * SPEED_HACK / 2.0f;
    else if (turn < 0.0f) currentTurnSpeed_ = -turnSpeed_ * SPEED_HACK / 2.0f;
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

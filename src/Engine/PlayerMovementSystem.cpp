// src/Engine/PlayerMovementSystem.cpp

#include "PlayerMovementSystem.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../Events/EventBus.h"
#include "../Input/InputMaster.h"
#include "../Physics/PhysicsSystem.h"
#include "../Terrain/Terrain.h"
#include <cmath>
#include <glm/glm.hpp>

PlayerMovementSystem::PlayerMovementSystem(entt::registry& registry)
    : registry_(registry)
{}

void PlayerMovementSystem::init() {
    EventBus::instance().subscribe<PlayerMoveCommandEvent>(
        [this](const PlayerMoveCommandEvent& cmd) {
            pendingCmd_ = cmd;
        });
}

void PlayerMovementSystem::update(float deltaTime) {
    auto view = registry_.view<TransformComponent, InputStateComponent>();
    for (auto entity : view) {
        auto& tc    = view.get<TransformComponent>(entity);
        auto& input = view.get<InputStateComponent>(entity);

        // --- Poll inputs (or apply EventBus command if EventBus mode is active) ---
        if (input.useEventBus) {
            // Replicate InputComponent::applyMovementCommand() logic using the
            // latest command stored by the EventBus handler.
            const auto& cmd = pendingCmd_;

            if      (cmd.forward > 0.0f) input.currentSpeed =  input.runSpeed * input.speedHack;
            else if (cmd.forward < 0.0f) input.currentSpeed = -input.runSpeed * input.speedHack;
            else                         input.currentSpeed = 0.0f;

            if      (cmd.turn > 0.0f) input.currentTurnSpeed =  input.turnSpeed * input.speedHack / 2.0f;
            else if (cmd.turn < 0.0f) input.currentTurnSpeed = -input.turnSpeed * input.speedHack / 2.0f;
            else                      input.currentTurnSpeed = 0.0f;

            if (cmd.sprint)      input.speedHack = 4.5f;
            if (cmd.sprintReset) input.speedHack = 1.0f;

            if (cmd.jump && !input.isInAir) {
                input.upwardsSpeed = InputStateComponent::kJumpPower;
                input.isInAir      = true;
            }
        } else {
            float fwd  = InputMaster::isKeyDown(W) ?  1.0f
                       : InputMaster::isKeyDown(S) ? -1.0f : 0.0f;
            float turn = InputMaster::isKeyDown(A) ?  1.0f
                       : InputMaster::isKeyDown(D) ? -1.0f : 0.0f;

            // applyMovementCommand equivalent
            if      (fwd > 0.0f) input.currentSpeed =  input.runSpeed * input.speedHack;
            else if (fwd < 0.0f) input.currentSpeed = -input.runSpeed * input.speedHack;
            else                 input.currentSpeed = 0.0f;

            if      (turn > 0.0f) input.currentTurnSpeed =  input.turnSpeed * input.speedHack / 2.0f;
            else if (turn < 0.0f) input.currentTurnSpeed = -input.turnSpeed * input.speedHack / 2.0f;
            else                  input.currentTurnSpeed = 0.0f;

            if (InputMaster::isKeyDown(Tab))       input.speedHack = 4.5f;
            if (InputMaster::isKeyDown(Backslash)) input.speedHack = 1.0f;

            if (InputMaster::isKeyDown(Space) && !input.isInAir) {
                input.upwardsSpeed = InputStateComponent::kJumpPower;
                input.isInAir      = true;
            }
        }

        // --- Apply yaw rotation ---
        tc.rotation.y += input.currentTurnSpeed * deltaTime;

        float sinY = std::sin(glm::radians(tc.rotation.y));
        float cosY = std::cos(glm::radians(tc.rotation.y));

        // --- Physics path ---
        if (input.physicsSystem) {
            input.physicsSystem->setPlayerWalkDirection(
                input.currentSpeed * sinY * deltaTime,
                input.currentSpeed * cosY * deltaTime,
                InputMaster::isKeyDown(Space));
            continue;
        }

        // --- Legacy path: manual gravity + terrain-height collision ---
        float distance = input.currentSpeed * deltaTime;
        tc.position.x += distance * sinY;
        tc.position.z += distance * cosY;

        input.upwardsSpeed += InputStateComponent::kGravity * deltaTime;
        tc.position.y += input.upwardsSpeed * deltaTime;

        if (input.terrain) {
            float terrainHeight = input.terrain->getHeightOfTerrain(tc.position.x, tc.position.z);
            if (tc.position.y <= terrainHeight) {
                input.upwardsSpeed = 0.0f;
                tc.position.y      = terrainHeight;
                input.isInAir      = false;
            }
        }
    }
}


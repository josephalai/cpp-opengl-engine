// src/Engine/PlayerMovementSystem.cpp

#include "PlayerMovementSystem.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../Events/EventBus.h"
#include "../Input/InputMaster.h"
#include "../Config/ConfigManager.h"
#include "../Physics/PhysicsSystem.h"
#include "../Terrain/Terrain.h"
#include "../Entities/Camera.h"
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

        // --- Read input axes (camera-relative movement) ---
        // fwdInput:    +1 = W (forward),  -1 = S (backward)
        // strafeInput: +1 = A (left),     -1 = D (right)
        // camYaw:      absolute camera orbit yaw in degrees
        float fwdInput    = 0.0f;
        float strafeInput = 0.0f;
        float camYaw      = 0.0f;
        bool  wantsJump   = false;

        if (input.useEventBus) {
            const auto& cmd = pendingCmd_;
            fwdInput    = cmd.forward;
            strafeInput = cmd.strafe;
            camYaw      = cmd.cameraYaw;
            wantsJump   = cmd.jump;

            if (cmd.sprint)      input.speedHack = ConfigManager::get().physics.sprintMultiplier;
            if (cmd.sprintReset) input.speedHack = 1.0f;

            if (wantsJump && !input.isInAir) {
                input.upwardsSpeed = InputStateComponent::kJumpPower;
                input.isInAir      = true;
            }
        } else {
            fwdInput    = InputMaster::isActionDown("MoveForward")  ?  1.0f
                        : InputMaster::isActionDown("MoveBackward") ? -1.0f : 0.0f;
            strafeInput = InputMaster::isActionDown("MoveLeft")     ?  1.0f
                        : InputMaster::isActionDown("MoveRight")    ? -1.0f : 0.0f;
            // Camera::Yaw is kept equal to PlayerCamera::orbitYaw_ each frame.
            camYaw      = Camera::Yaw;
            wantsJump   = InputMaster::isActionDown("Jump");

            if (InputMaster::isActionDown("Sprint"))      input.speedHack = ConfigManager::get().physics.sprintMultiplier;
            if (InputMaster::isActionDown("SprintReset")) input.speedHack = 1.0f;

            if (wantsJump && !input.isInAir) {
                input.upwardsSpeed = InputStateComponent::kJumpPower;
                input.isInAir      = true;
            }
        }

        // --- Compute camera-relative movement vector ---
        //
        // Convention (matches SharedMovement::applyInput):
        //   forward component : (sin(camYaw), cos(camYaw))
        //   strafe left  (A)  : A key → strafeInput = +1 → strafeSpeed_v = -speed
        //                       dx = strafeSpeed_v * cosY,  dz = -strafeSpeed_v * sinY
        //
        // Combined per-frame displacement (before scaling by deltaTime):
        //   dx = fwdSpeed * sinY + strafeSpeed_v * cosY
        //   dz = fwdSpeed * cosY - strafeSpeed_v * sinY
        //
        // This is identical to what SharedMovement::applyInput computes on the
        // server, so client-side prediction stays in sync with the server.
        const float speed      = input.runSpeed * input.speedHack;
        const float fwdSpeed   = fwdInput * speed;
        // A (strafeInput = +1) → move left → SharedMovement: strafeSpeed = -speed
        const float strafeSpeed_v = -strafeInput * speed;

        const float sinY = std::sin(glm::radians(camYaw));
        const float cosY = std::cos(glm::radians(camYaw));

        const float totalDx = fwdSpeed * sinY + strafeSpeed_v * cosY;
        const float totalDz = fwdSpeed * cosY - strafeSpeed_v * sinY;

        // --- Snap player model to face movement direction ---
        // If the player is moving, instantly orient the character model to face
        // the direction of travel (camera-relative "Action RPG" feel).
        if (totalDx * totalDx + totalDz * totalDz > 1e-4f) {
            tc.rotation.y = glm::degrees(std::atan2(totalDx, totalDz));
        }

        // --- Physics path ---
        if (input.physicsSystem) {
            input.physicsSystem->setPlayerWalkDirection(
                totalDx * deltaTime,
                totalDz * deltaTime,
                wantsJump);
            continue;
        }

        // --- Legacy path: manual gravity + terrain-height collision ---
        tc.position.x += totalDx * deltaTime;
        tc.position.z += totalDz * deltaTime;

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

    // Phase 4 Step 4.2 — Origin Shift.
    // When the player exceeds kOriginShiftThreshold from the local origin,
    // subtract the player offset from every entity so float precision stays
    // tight around (0,0,0) in render space.
    auto tcView = registry_.view<TransformComponent, InputStateComponent>();
    for (auto entity : tcView) {
        auto& tc = tcView.get<TransformComponent>(entity);
        float dist = std::sqrt(tc.position.x * tc.position.x +
                               tc.position.z * tc.position.z);
        if (dist > kOriginShiftThreshold) {
            glm::vec3 shift = tc.position;
            shift.y = 0.0f; // only shift XZ
            auto allTransforms = registry_.view<TransformComponent>();
            for (auto e : allTransforms) {
                auto& t = allTransforms.get<TransformComponent>(e);
                t.position -= shift;
            }
            break; // one shift per frame
        }
    }
}

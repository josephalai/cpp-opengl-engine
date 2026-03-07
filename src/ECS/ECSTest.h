#ifndef ECS_ECSTEST_H
#define ECS_ECSTEST_H

// src/ECS/ECSTest.h
//
// Phase 2 Step 1 — Standalone EnTT registry verification utility.
//
// Call ECSTest::run() from Engine::init() (or any other context) to verify
// that the EnTT registry is functional: entity creation, component emplacement,
// view iteration, and entity destruction all work correctly.
//
// This file is intentionally self-contained (no Engine dependency) so it can
// be included in unit tests or server builds independently.

#include <entt/entt.hpp>
#include <iostream>
#include <cassert>

class ECSTest {
public:
    /// Run a thorough registry verification.
    /// Prints results to stdout and asserts on failure.
    static void run() {
        entt::registry reg;

        struct Position { float x, y, z; };
        struct Velocity { float dx, dy, dz; };

        // Create multiple entities with different component combinations.
        auto e0 = reg.create();
        reg.emplace<Position>(e0, 0.0f, 0.0f, 0.0f);
        reg.emplace<Velocity>(e0, 1.0f, 0.0f, 0.0f);

        auto e1 = reg.create();
        reg.emplace<Position>(e1, 10.0f, 5.0f, 3.0f);

        auto e2 = reg.create();
        reg.emplace<Position>(e2, 20.0f, 0.0f, 0.0f);
        reg.emplace<Velocity>(e2, 0.0f, 2.0f, 0.0f);

        // Verify component retrieval.
        auto& pos0 = reg.get<Position>(e0);
        assert(pos0.x == 0.0f && pos0.y == 0.0f && pos0.z == 0.0f);

        auto& pos1 = reg.get<Position>(e1);
        assert(pos1.x == 10.0f && pos1.y == 5.0f && pos1.z == 3.0f);

        // Verify view iteration over entities with both Position and Velocity.
        int movingCount = 0;
        auto view = reg.view<Position, Velocity>();
        for (auto entity : view) {
            auto& p = view.get<Position>(entity);
            auto& v = view.get<Velocity>(entity);
            p.x += v.dx;
            p.y += v.dy;
            ++movingCount;
        }
        assert(movingCount == 2); // e0 and e2 have both components

        // Verify mutation was applied.
        assert(reg.get<Position>(e0).x == 1.0f);
        assert(reg.get<Position>(e2).y == 2.0f);

        // Verify entity destruction.
        reg.destroy(e1);
        assert(!reg.valid(e1));
        assert(reg.valid(e0));
        assert(reg.valid(e2));

        // Verify view only sees surviving entities.
        int posCount = 0;
        reg.view<Position>().each([&](auto, const Position&) { ++posCount; });
        assert(posCount == 2);

        std::cout << "[ECS] ECSTest::run() passed — "
                  << posCount << " entities with Position after destruction.\n";
    }
};

#endif // ECS_ECSTEST_H

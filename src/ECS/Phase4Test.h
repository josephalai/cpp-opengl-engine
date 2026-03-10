// src/ECS/Phase4Test.h
//
// Phase 4 — Standalone verification of the spatial partitioning, pathfinding,
// LOD, and origin-shift systems.  Call Phase4Test::run() from Engine::init()
// or the server build to verify all Phase 4 data structures work correctly.

#ifndef ECS_PHASE4TEST_H
#define ECS_PHASE4TEST_H

#include <entt/entt.hpp>
#include <iostream>
#include <cassert>
#include <cmath>
#include <glm/glm.hpp>

#include "../Streaming/SpatialGrid.h"
#include "../ECS/Components/SpatialComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/PathfindingComponent.h"
#include "../ECS/Components/LODComponent.h"
#include "../ECS/Components/OriginShiftComponent.h"
#include "../Engine/SpatialSystem.h"
#include "../Engine/PathfindingSystem.h"
#include "../Engine/LODSystem.h"
#include "../Navigation/NavMeshManager.h"
#include "../Terrain/TerrainLOD.h"

class Phase4Test {
public:
    static void run() {
        testSpatialGrid();
        testSpatialComponent();
        testSpatialSystem();
        testPathfinding();
        testNavMesh();
        testLODSystem();
        testOriginShift();
        testTerrainLOD();

        std::cout << "[Phase4] All Phase 4 tests passed.\n";
    }

private:
    static void testSpatialGrid() {
        SpatialGrid grid(50.0f);

        // Test worldToCell conversion.
        int cx, cz;
        grid.worldToCell(25.0f, 75.0f, cx, cz);
        assert(cx == 0 && cz == 1);

        grid.worldToCell(100.0f, 200.0f, cx, cz);
        assert(cx == 2 && cz == 4);

        grid.worldToCell(-10.0f, -60.0f, cx, cz);
        assert(cx == -1 && cz == -2);

        // Test addEntity / queryNeighbourhood.
        entt::registry reg;
        auto e0 = reg.create();
        auto e1 = reg.create();
        auto e2 = reg.create();

        grid.addEntity(e0, 0, 0, true);   // Static in (0,0)
        grid.addEntity(e1, 0, 0, false);  // Dynamic in (0,0)
        grid.addEntity(e2, 1, 0, false);  // Dynamic in (1,0)

        auto result = grid.queryNeighbourhood(0, 0);
        assert(result.size() == 3);  // e0, e1, e2 all in 3×3 of (0,0)

        // Query from a distant cell should not find any of these.
        auto farResult = grid.queryNeighbourhood(10, 10);
        assert(farResult.empty());

        // Test removeEntity.
        grid.removeEntity(e0, 0, 0);
        result = grid.queryNeighbourhood(0, 0);
        assert(result.size() == 2);

        // Test migrateEntity.
        grid.migrateEntity(e1, 0, 0, 5, 5, false);
        result = grid.queryNeighbourhood(0, 0);
        assert(result.size() == 1);  // Only e2 left near (0,0)

        result = grid.queryNeighbourhood(5, 5);
        assert(result.size() == 1);  // e1 moved to (5,5)

        std::cout << "[Phase4] SpatialGrid tests passed.\n";
    }

    static void testSpatialComponent() {
        entt::registry reg;
        auto entity = reg.create();
        reg.emplace<SpatialComponent>(entity, SpatialComponent{3, 7});

        auto& sc = reg.get<SpatialComponent>(entity);
        assert(sc.currentCellX == 3);
        assert(sc.currentCellZ == 7);

        std::cout << "[Phase4] SpatialComponent tests passed.\n";
    }

    static void testSpatialSystem() {
        entt::registry reg;
        SpatialGrid grid(50.0f);
        SpatialSystem sys(reg, grid);

        // Create an entity at position (25, 0, 25) → cell (0, 0).
        auto e = reg.create();
        reg.emplace<TransformComponent>(e, TransformComponent{{25.0f, 0.0f, 25.0f}});
        reg.emplace<NetworkIdComponent>(e);

        sys.registerEntity(e, glm::vec3(25.0f, 0.0f, 25.0f), false);

        auto& sc = reg.get<SpatialComponent>(e);
        assert(sc.currentCellX == 0 && sc.currentCellZ == 0);

        // Move the entity to (125, 0, 75) → cell (2, 1).
        reg.get<TransformComponent>(e).position = {125.0f, 0.0f, 75.0f};
        sys.update(0.1f);

        assert(sc.currentCellX == 2 && sc.currentCellZ == 1);

        // Verify the entity is now in the correct cell.
        auto result = grid.queryNeighbourhood(2, 1);
        assert(result.size() == 1);
        assert(result[0] == e);

        std::cout << "[Phase4] SpatialSystem tests passed.\n";
    }

    static void testPathfinding() {
        entt::registry reg;
        PathfindingSystem sys(reg, 10.0f);

        auto e = reg.create();
        reg.emplace<TransformComponent>(e, TransformComponent{{0.0f, 0.0f, 0.0f}});

        // Set up a simple 3-waypoint path.
        PathfindingComponent pc;
        pc.waypoints = {
            {10.0f, 0.0f, 0.0f},
            {20.0f, 0.0f, 0.0f},
            {30.0f, 0.0f, 0.0f}
        };
        pc.arrivalRadius = 0.5f;
        reg.emplace<PathfindingComponent>(e, pc);

        // Run enough steps to reach the first waypoint.
        for (int i = 0; i < 20; ++i) {
            sys.update(0.1f);  // 2 seconds total, speed=10 → 20m
            if (!reg.any_of<PathfindingComponent>(e)) break;
        }

        auto& tc = reg.get<TransformComponent>(e);
        // The entity should have moved significantly toward the first waypoint.
        assert(tc.position.x > 5.0f);

        std::cout << "[Phase4] PathfindingSystem tests passed.\n";
    }

    static void testNavMesh() {
        NavMeshManager nav(1.0f);
        nav.build(0.0f, 0.0f, 100.0f, 100.0f);

        // Path from (5,0,5) to (95,0,95) should succeed on open grid.
        auto path = nav.findPath({5, 0, 5}, {95, 0, 95});
        assert(!path.empty());
        assert(path.size() > 2);

        // Add an obstacle across the middle.
        uint32_t obs = nav.addObstacle(40.0f, 0.0f, 60.0f, 100.0f);

        // Path should still find a way around (or fail if completely blocked).
        auto path2 = nav.findPath({5, 0, 50}, {95, 0, 50});
        // The obstacle blocks X=40..60 for all Z, so the path must go around.
        // It should either find an alternate route or return empty.
        // With X=40..60 blocked for ALL Z=0..100, there's no way through.
        assert(path2.empty());

        // Remove obstacle — path should work again.
        nav.removeObstacle(obs);
        auto path3 = nav.findPath({5, 0, 50}, {95, 0, 50});
        assert(!path3.empty());

        std::cout << "[Phase4] NavMesh tests passed.\n";
    }

    static void testLODSystem() {
        entt::registry reg;
        LODSystem sys(reg);

        auto e = reg.create();
        reg.emplace<TransformComponent>(e, TransformComponent{{100.0f, 0.0f, 0.0f}});
        reg.emplace<LODComponent>(e, LODComponent{0, 20.0f, 60.0f});

        // Camera at origin — distance = 100 m → LOD2.
        sys.update(glm::vec3(0.0f));
        assert(reg.get<LODComponent>(e).currentLOD == 2);

        // Camera at 90m — distance = 10 m → LOD0.
        sys.update(glm::vec3(90.0f, 0.0f, 0.0f));
        assert(reg.get<LODComponent>(e).currentLOD == 0);

        // Camera at 60m — distance = 40 m → LOD1.
        sys.update(glm::vec3(60.0f, 0.0f, 0.0f));
        assert(reg.get<LODComponent>(e).currentLOD == 1);

        std::cout << "[Phase4] LODSystem tests passed.\n";
    }

    static void testOriginShift() {
        OriginShiftComponent oc;
        oc.chunkIndexX = 1;
        oc.chunkIndexZ = 2;
        oc.localOffset = {50.0f, 10.0f, 30.0f};

        double ax = oc.absoluteX(800.0f);
        double az = oc.absoluteZ(800.0f);
        assert(std::abs(ax - 850.0) < 0.01);
        assert(std::abs(az - 1630.0) < 0.01);

        std::cout << "[Phase4] OriginShift tests passed.\n";
    }

    static void testTerrainLOD() {
        TerrainLOD lod;
        lod.build(17, 800.0f, 50.0f);  // 17 vertices, 800m terrain, 50m patches
        assert(lod.isBuilt());
        assert(lod.patchesPerRow() == 16);

        // LOD0 should have more indices than LOD2.
        auto& lod0 = lod.getIndicesForPatch(0, 0, 0);
        auto& lod2 = lod.getIndicesForPatch(0, 0, 2);
        assert(!lod0.empty());
        assert(lod0.size() >= lod2.size());

        // LOD selection based on distance.
        int level = lod.selectLOD(25.0f, 25.0f, 25.0f, 25.0f);
        assert(level == 0);  // Same position → full detail.

        level = lod.selectLOD(25.0f, 25.0f, 500.0f, 500.0f);
        assert(level == 2);  // Very far → lowest detail.

        std::cout << "[Phase4] TerrainLOD tests passed.\n";
    }
};

#endif // ECS_PHASE4TEST_H

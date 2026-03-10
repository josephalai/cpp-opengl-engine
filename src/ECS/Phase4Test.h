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
#include "../Engine/GLUploadQueue.h"
#include "../Navigation/NavMeshManager.h"
#include "../Terrain/TerrainData.h"
#include "../Terrain/TerrainLOD.h"
#include "../Physics/PhysicsSystem.h"

// Terrain.h pulls in OpenGL headers, so only include on client builds.
#ifndef HEADLESS_SERVER
#include "../Terrain/Terrain.h"
#endif

class Phase4Test {
public:
    static void run() {
        testSpatialGrid();
        testSpatialComponent();
        testSpatialSystem();
        testPathfinding();
        testNavMesh();
        testNavMeshDynamicObstacles();
        testLODSystem();
        testOriginShift();
        testOriginShiftTransform();
        testTerrainLOD();
        testTerrainDataStruct();
        testGLUploadQueueDrain();
        testPhysicsTerrainColliderRemoval();
        testMultiTileFilenames();

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

    /// Phase 4 Step 4.4 — Test dynamic obstacle via addObstacleFromBounds()
    /// and rebuildTile().
    static void testNavMeshDynamicObstacles() {
        NavMeshManager nav(1.0f);
        nav.build(0.0f, 0.0f, 100.0f, 100.0f);

        // Path from (5,0,50) to (95,0,50) should succeed on open grid.
        auto path = nav.findPath({5, 0, 50}, {95, 0, 50});
        assert(!path.empty());

        // Add a building via bounding box convenience method.
        // center=(50,0,50), halfExtents=(10,5,60) → blocks X=[40..60], Z=[-10..110]
        // This wide Z-range ensures the obstacle fully blocks the X=40..60 corridor.
        uint32_t obs = nav.addObstacleFromBounds(
            glm::vec3(50.0f, 0.0f, 50.0f),  // center
            glm::vec3(10.0f, 5.0f, 60.0f));  // half-extents (blocks X=40..60, Z=-10..110)

        // Call rebuildTile (no-op for grid-based pathfinder, but exercises the API).
        nav.rebuildTile(40.0f, -10.0f, 60.0f, 110.0f);

        // Direct path should now be blocked.
        auto path2 = nav.findPath({5, 0, 50}, {95, 0, 50});
        assert(path2.empty());

        // Remove obstacle — path should work again.
        nav.removeObstacle(obs);
        auto path3 = nav.findPath({5, 0, 50}, {95, 0, 50});
        assert(!path3.empty());

        std::cout << "[Phase4] NavMesh dynamic obstacle tests passed.\n";
    }

    /// Phase 4 Step 4.2 — Test TransformComponent double-precision accessors.
    static void testOriginShiftTransform() {
        TransformComponent tc;
        tc.position = glm::vec3(100.0f, 50.0f, 200.0f);
        glm::dvec3 origin(0.0);

        // Sync to absolute
        tc.syncToAbsolute(origin);
        assert(std::abs(tc.absolutePosition.x - 100.0) < 0.01);
        assert(std::abs(tc.absolutePosition.z - 200.0) < 0.01);

        // Apply origin shift and sync back
        glm::dvec3 shift(1000.0, 0.0, 2000.0);
        tc.syncToAbsolute(shift);
        assert(std::abs(tc.absolutePosition.x - 1100.0) < 0.01);
        assert(std::abs(tc.absolutePosition.z - 2200.0) < 0.01);

        // Sync from absolute back to float
        tc.syncFromAbsolute(shift);
        assert(std::abs(tc.position.x - 100.0f) < 0.01f);
        assert(std::abs(tc.position.z - 200.0f) < 0.01f);

        std::cout << "[Phase4] OriginShift TransformComponent tests passed.\n";
    }

    /// Phase 4 Step 4.2 — Test that TerrainData struct is correctly default
    /// initialised.
    static void testTerrainDataStruct() {
        // Default-constructed TerrainData should be invalid.
        TerrainData td;
        assert(!td.valid);
        assert(td.vertices.empty());
        assert(td.normals.empty());
        assert(td.indices.empty());
        assert(td.gridX == 0 && td.gridZ == 0);
        assert(td.originX == 0.0f && td.originZ == 0.0f);

        // Populated TerrainData should carry its fields.
        TerrainData td2;
        td2.gridX  = 3;
        td2.gridZ  = 5;
        td2.originX = 2400.0f;
        td2.originZ = 4000.0f;
        td2.vertices = {1.0f, 2.0f, 3.0f};
        td2.valid = true;
        assert(td2.valid);
        assert(td2.vertices.size() == 3);
        assert(td2.gridX == 3);

        std::cout << "[Phase4] TerrainData struct tests passed.\n";
    }

    /// Phase 4 Step 4.2 — Test GLUploadQueue can drain tasks.
    static void testGLUploadQueueDrain() {
        // The GLUploadQueue is used on the main (GL) thread. We test
        // the enqueue + processAll flow with simple lambdas.
        int counter = 0;
        GLUploadQueue::instance().enqueue([&counter]() { counter += 1; });
        GLUploadQueue::instance().enqueue([&counter]() { counter += 10; });
        GLUploadQueue::instance().processAll(0); // drain all
        assert(counter == 11);

        // Test maxPerFrame limiting.
        GLUploadQueue::instance().enqueue([&counter]() { counter += 100; });
        GLUploadQueue::instance().enqueue([&counter]() { counter += 1000; });
        GLUploadQueue::instance().processAll(1); // process only 1
        assert(counter == 111);
        GLUploadQueue::instance().processAll(0); // drain remaining
        assert(counter == 1111);

        std::cout << "[Phase4] GLUploadQueue drain tests passed.\n";
    }

    /// Phase 4 — Test PhysicsSystem terrain collider add/remove.
    static void testPhysicsTerrainColliderRemoval() {
        // Matches Terrain::kSize — can't reference directly in headless builds
        // because Terrain.h pulls in GL headers.
        static constexpr float kTerrainSize = 800.0f;

        PhysicsSystem physics;
        entt::registry reg;
        physics.setRegistry(reg);
        physics.init();

        // Create a small 4×4 height grid.
        std::vector<std::vector<float>> heights(4, std::vector<float>(4, 0.0f));
        heights[1][1] = 5.0f;
        heights[2][2] = 3.0f;

        // Add two terrain colliders at different grid positions.
        physics.addHeadlessTerrainCollider(heights, kTerrainSize, 0.0f, 0.0f);                // grid (0,0)
        physics.addHeadlessTerrainCollider(heights, kTerrainSize, kTerrainSize, 0.0f);         // grid (1,0)

        // The Bullet world should have rigid bodies.
        assert(physics.getWorld()->getNumCollisionObjects() >= 2);

        // Remove one terrain collider.
        int beforeCount = physics.getWorld()->getNumCollisionObjects();
        physics.removeHeadlessTerrainCollider(0, 0);
        int afterCount = physics.getWorld()->getNumCollisionObjects();
        assert(afterCount == beforeCount - 1);

        // Removing a non-existent collider should be a no-op.
        physics.removeHeadlessTerrainCollider(99, 99);
        assert(physics.getWorld()->getNumCollisionObjects() == afterCount);

        physics.shutdown();
        std::cout << "[Phase4] Physics terrain collider removal tests passed.\n";
    }

    /// Phase 4 Step 3 — Test that Terrain::parseCPU multi-tile filename
    /// logic correctly derives per-tile names.
    /// Only available on client builds (Terrain.h requires GL headers).
    static void testMultiTileFilenames() {
        // Matches Terrain::kSize — can't reference directly in headless builds.
        static constexpr float kTerrainSize = 800.0f;
#ifndef HEADLESS_SERVER
        // parseCPU should try "heightMap_X_Z" first, then fall back to base.
        // Since we may not have actual files in CI, we just verify the method
        // runs without crashing and returns a TerrainData with valid=false
        // when no heightmap file exists.
        TerrainData td = Terrain::parseCPU(99, 99, "nonexistent_heightmap");
        assert(!td.valid);
        assert(td.gridX == 99);
        assert(td.gridZ == 99);
        assert(td.originX == 99 * kTerrainSize);
        assert(td.originZ == 99 * kTerrainSize);

        std::cout << "[Phase4] Multi-tile filename tests passed.\n";
#else
        (void)kTerrainSize;
        std::cout << "[Phase4] Multi-tile filename tests skipped (headless build).\n";
#endif
    }
};

#endif // ECS_PHASE4TEST_H

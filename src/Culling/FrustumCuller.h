// src/Culling/FrustumCuller.h
// High-level culling helper: updates the frustum from the camera each frame
// and filters entity/terrain lists to only the visible subset.
//
// Phase 4 Step 4.1.5 — Cell-level frustum culling: test SpatialGrid cells
// (50 m × 50 m) against the frustum and skip iterating entities inside
// cells that are entirely outside the camera's view.

#ifndef ENGINE_FRUSTUMCULLER_H
#define ENGINE_FRUSTUMCULLER_H

#include "Frustum.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include <vector>
#include <glm/glm.hpp>

class Camera;
class Entity;
class Terrain;

class FrustumCuller {
public:
    /// Recompute the frustum planes from the current camera view and projection.
    void update(Camera* camera, const glm::mat4& projectionMatrix);

    /// Return only the entities whose world-space AABB intersects the frustum.
    /// Entities without a valid BoundingBox AABB are always included.
    std::vector<Entity*> cull(const std::vector<Entity*>& entities) const;

    /// Same for Assimp-loaded scene components.
    std::vector<AssimpModelComponent> cull(const std::vector<AssimpModelComponent>& comps) const;

    /// Return only the terrain tiles whose world-space extent intersects the frustum.
    /// Each tile occupies [x, x+800] x [-500, 500] x [z, z+800].
    std::vector<Terrain*> cullTerrains(const std::vector<Terrain*>& terrains) const;

    /// Phase 4 Step 4.1.5 — Test whether a SpatialGrid cell (AABB) is visible
    /// in the camera's view frustum.  Each cell spans [cellX*size, (cellX+1)*size]
    /// on X and [cellZ*size, (cellZ+1)*size] on Z, with a generous Y range.
    bool isCellVisible(int cellX, int cellZ, float cellSize,
                       float yMin = -500.0f, float yMax = 500.0f) const {
        glm::vec3 aabbMin(cellX * cellSize, yMin, cellZ * cellSize);
        glm::vec3 aabbMax((cellX + 1) * cellSize, yMax, (cellZ + 1) * cellSize);
        return frustum_.isAABBVisible(aabbMin, aabbMax);
    }

    /// Access the underlying frustum for direct queries.
    const Frustum& frustum() const { return frustum_; }

private:
    Frustum frustum_;
};

#endif // ENGINE_FRUSTUMCULLER_H

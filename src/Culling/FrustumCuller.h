// src/Culling/FrustumCuller.h
// High-level culling helper: updates the frustum from the camera each frame
// and filters entity/terrain lists to only the visible subset.

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
    std::vector<AssimpModelComponent> cull(const std::vector<AssimpModelComponent>& components) const;

    /// Return only the terrain tiles whose world-space extent intersects the frustum.
    /// Each tile occupies [x, x+800] x [-500, 500] x [z, z+800].
    std::vector<Terrain*> cullTerrains(const std::vector<Terrain*>& terrains) const;

private:
    Frustum frustum_;
};

#endif // ENGINE_FRUSTUMCULLER_H

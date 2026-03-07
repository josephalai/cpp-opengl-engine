// src/Culling/FrustumCuller.cpp

#include "FrustumCuller.h"
#include "../Entities/Camera.h"
#include "../Entities/Entity.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../Terrain/Terrain.h"
#include "../BoundingBox/BoundingBox.h"
#include <glm/glm.hpp>

/// Approximate vertical extent used for terrain AABB tests.
/// Encompasses the full range of terrain heights including sub-sea trenches
/// and tall peaks in the heightmap.
static constexpr float kTerrainMinY = -500.0f;
static constexpr float kTerrainMaxY =  500.0f;

void FrustumCuller::update(Camera* camera, const glm::mat4& projectionMatrix) {
    glm::mat4 vp = projectionMatrix * camera->getViewMatrix();
    frustum_.update(vp);
}

std::vector<Entity*> FrustumCuller::cull(const std::vector<Entity*>& entities) const {
    std::vector<Entity*> visible;
    visible.reserve(entities.size());

    for (Entity* e : entities) {
        if (!e) continue;

        BoundingBox* box = e->getBoundingBox();
        if (!box) {
            // No bounding data — always include.
            visible.push_back(e);
            continue;
        }

        BoundingAABB aabb = box->getAABB();
        if (!aabb.valid) {
            // AABB not set — always include.
            visible.push_back(e);
            continue;
        }

        // Transform the local-space AABB by the entity's world position and scale.
        // NOTE: entity rotation is intentionally ignored here — the spec asks for
        // a simple position+scale offset. If tight culling for rotated entities is
        // needed in the future, compute the OBB-to-AABB expansion instead.
        float scale = e->getScale();
        glm::vec3 pos = e->getPosition();
        glm::vec3 worldMin = pos + aabb.min * scale;
        glm::vec3 worldMax = pos + aabb.max * scale;

        if (frustum_.isAABBVisible(worldMin, worldMax)) {
            visible.push_back(e);
        }
    }
    return visible;
}

std::vector<AssimpModelComponent> FrustumCuller::cull(const std::vector<AssimpModelComponent>& comps) const {
    std::vector<AssimpModelComponent> visible;
    visible.reserve(comps.size());

    for (const AssimpModelComponent& comp : comps) {
        if (!comp.mesh) {
            // Component without a mesh is invalid — skip it.
            continue;
        }

        if (!comp.box) {
            visible.push_back(comp);
            continue;
        }

        BoundingAABB aabb = comp.box->getAABB();
        if (!aabb.valid) {
            visible.push_back(comp);
            continue;
        }

        // Transform local AABB by position and scale (rotation ignored per spec — see Entity cull above).
        glm::vec3 worldMin = comp.position + aabb.min * comp.scale;
        glm::vec3 worldMax = comp.position + aabb.max * comp.scale;

        if (frustum_.isAABBVisible(worldMin, worldMax)) {
            visible.push_back(comp);
        }
    }
    return visible;
}

std::vector<Terrain*> FrustumCuller::cullTerrains(const std::vector<Terrain*>& terrains) const {
    std::vector<Terrain*> visible;
    visible.reserve(terrains.size());

    for (Terrain* t : terrains) {
        if (!t) continue;

        float x    = t->getX();
        float z    = t->getZ();
        float size = t->getSize();

        glm::vec3 minPt(x,        kTerrainMinY, z);
        glm::vec3 maxPt(x + size, kTerrainMaxY, z + size);

        if (frustum_.isAABBVisible(minPt, maxPt)) {
            visible.push_back(t);
        }
    }
    return visible;
}

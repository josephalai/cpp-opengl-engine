// src/Interaction/EntityPicker.cpp
//
// Ray-AABB intersection picker — client-only, compiled out on headless server.

#include "EntityPicker.h"

#ifndef HEADLESS_SERVER

#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/StaticModelComponent.h"
#include "../ECS/Components/ColliderComponent.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../BoundingBox/BoundingBox.h"

#include <limits>
#include <glm/glm.hpp>

// -------------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------------

static constexpr float kRayDirEpsilon = 1e-30f; ///< Guards against divide-by-zero in slab test.

EntityPicker::EntityPicker(entt::registry& registry)
    : registry_(registry)
{}

// -------------------------------------------------------------------------
// rayAABB — slab intersection test
// -------------------------------------------------------------------------

bool EntityPicker::rayAABB(const glm::vec3& rayOrigin,
                            const glm::vec3& rayDirInv,
                            const glm::vec3& boxMin,
                            const glm::vec3& boxMax,
                            float&           tHit)
{
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax =  std::numeric_limits<float>::infinity();

    for (int i = 0; i < 3; ++i) {
        float t1 = (boxMin[i] - rayOrigin[i]) * rayDirInv[i];
        float t2 = (boxMax[i] - rayOrigin[i]) * rayDirInv[i];
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmax < 0.0f || tmin > tmax) return false;
    }

    tHit = (tmin >= 0.0f) ? tmin : tmax;
    return tHit >= 0.0f;
}

// -------------------------------------------------------------------------
// pick — return closest entity hit by the ray
// -------------------------------------------------------------------------

entt::entity EntityPicker::pick(const glm::vec3& rayOrigin,
                                 const glm::vec3& rayDirection) const
{
    const glm::vec3 rayDirInv(
        1.0f / (rayDirection.x != 0.0f ? rayDirection.x : kRayDirEpsilon),
        1.0f / (rayDirection.y != 0.0f ? rayDirection.y : kRayDirEpsilon),
        1.0f / (rayDirection.z != 0.0f ? rayDirection.z : kRayDirEpsilon));

    entt::entity closest   = entt::null;
    float        closestT  = std::numeric_limits<float>::max();

    auto testBox = [&](entt::entity entity, BoundingBox* bb,
                       const glm::vec3& worldPos) {
        if (!bb) return;
        BoundingAABB aabb = bb->getAABB();
        if (!aabb.valid) return;

        glm::vec3 worldMin = aabb.min + worldPos;
        glm::vec3 worldMax = aabb.max + worldPos;

        float tHit = 0.0f;
        if (rayAABB(rayOrigin, rayDirInv, worldMin, worldMax, tHit)) {
            if (tHit < closestT) {
                closestT = tHit;
                closest  = entity;
            }
        }
    };

    // --- StaticModelComponent ---
    auto staticView = registry_.view<TransformComponent, StaticModelComponent>();
    for (auto e : staticView) {
        auto& tc  = staticView.get<TransformComponent>(e);
        auto& smc = staticView.get<StaticModelComponent>(e);
        testBox(e, smc.boundingBox, tc.position);
    }

    // --- ColliderComponent (used for network-spawned NPCs) ---
    auto colliderView = registry_.view<TransformComponent, ColliderComponent>();
    for (auto e : colliderView) {
        auto& tc  = colliderView.get<TransformComponent>(e);
        auto& cc  = colliderView.get<ColliderComponent>(e);
        testBox(e, cc.box, tc.position);
    }

    // --- AssimpModelComponent ---
    auto assimpView = registry_.view<TransformComponent, AssimpModelComponent>();
    for (auto e : assimpView) {
        auto& tc  = assimpView.get<TransformComponent>(e);
        auto& amc = assimpView.get<AssimpModelComponent>(e);
        testBox(e, amc.box, tc.position);
    }

    return closest;
}

#endif // !HEADLESS_SERVER

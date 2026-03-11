// src/Interaction/EntityPicker.cpp
//
// Client-side Ray-AABB entity picker.

#ifndef HEADLESS_SERVER

#include "EntityPicker.h"

#include "../BoundingBox/BoundingBox.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/StaticModelComponent.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/ColliderComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <limits>

// -------------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------------

EntityPicker::EntityPicker(entt::registry& registry)
    : registry_(registry) {}

// -------------------------------------------------------------------------
// pick — Ray-AABB intersection against all scene entities
// -------------------------------------------------------------------------

entt::entity EntityPicker::pick(const glm::vec3& rayOrigin,
                                const glm::vec3& rayDir) const {
    entt::entity closest = entt::null;
    float        minDist  = std::numeric_limits<float>::max();

    // Test entities with StaticModelComponent (have a BoundingBox*).
    {
        auto view = registry_.view<TransformComponent, StaticModelComponent>();
        for (auto entity : view) {
            const auto& tc  = view.get<TransformComponent>(entity);
            const auto& smc = view.get<StaticModelComponent>(entity);

            if (!smc.boundingBox) continue;
            BoundingAABB aabb = smc.boundingBox->getAABB();
            if (!aabb.valid) continue;

            // Transform local-space AABB to world space.
            glm::vec3 bmin = tc.position + aabb.min * tc.scale;
            glm::vec3 bmax = tc.position + aabb.max * tc.scale;

            float dist = rayAABB(rayOrigin, rayDir, bmin, bmax);
            if (dist >= 0.0f && dist < minDist) {
                minDist = dist;
                closest = entity;
            }
        }
    }

    // Test entities with ColliderComponent (have a BoundingBox*).
    {
        auto view = registry_.view<TransformComponent, ColliderComponent>();
        for (auto entity : view) {
            const auto& tc  = view.get<TransformComponent>(entity);
            const auto& cc  = view.get<ColliderComponent>(entity);

            if (!cc.box) continue;
            BoundingAABB aabb = cc.box->getAABB();
            if (!aabb.valid) continue;

            glm::vec3 bmin = tc.position + aabb.min * tc.scale;
            glm::vec3 bmax = tc.position + aabb.max * tc.scale;

            float dist = rayAABB(rayOrigin, rayDir, bmin, bmax);
            if (dist >= 0.0f && dist < minDist) {
                minDist = dist;
                closest = entity;
            }
        }
    }

    // Test entities with AssimpModelComponent (have a box field).
    {
        auto view = registry_.view<TransformComponent, AssimpModelComponent>();
        for (auto entity : view) {
            const auto& tc  = view.get<TransformComponent>(entity);
            const auto& amc = view.get<AssimpModelComponent>(entity);

            if (!amc.box) continue;
            BoundingAABB aabb = amc.box->getAABB();
            if (!aabb.valid) continue;

            glm::vec3 bmin = tc.position + aabb.min * tc.scale;
            glm::vec3 bmax = tc.position + aabb.max * tc.scale;

            float dist = rayAABB(rayOrigin, rayDir, bmin, bmax);
            if (dist >= 0.0f && dist < minDist) {
                minDist = dist;
                closest = entity;
            }
        }
    }

    return closest;
}

// -------------------------------------------------------------------------
// buildPickRay — unproject mouse position into a world-space ray
// -------------------------------------------------------------------------

void EntityPicker::buildPickRay(float mouseX, float mouseY,
                                int screenW, int screenH,
                                const glm::mat4& projection, const glm::mat4& view,
                                glm::vec3& outOrigin, glm::vec3& outDir) {
    // Normalised Device Coordinates.
    float ndcX =  (2.0f * mouseX / static_cast<float>(screenW)) - 1.0f;
    float ndcY = -(2.0f * mouseY / static_cast<float>(screenH)) + 1.0f;

    glm::mat4 invVP = glm::inverse(projection * view);

    glm::vec4 nearClip(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farClip (ndcX, ndcY,  1.0f, 1.0f);

    glm::vec4 nearWorld = invVP * nearClip;
    glm::vec4 farWorld  = invVP * farClip;

    nearWorld /= nearWorld.w;
    farWorld  /= farWorld.w;

    outOrigin = glm::vec3(nearWorld);
    outDir    = glm::normalize(glm::vec3(farWorld) - outOrigin);
}

// -------------------------------------------------------------------------
// rayAABB — slab method
// -------------------------------------------------------------------------

float EntityPicker::rayAABB(const glm::vec3& origin, const glm::vec3& dir,
                            const glm::vec3& boxMin, const glm::vec3& boxMax) {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-6f) {
            if (origin[i] < boxMin[i] || origin[i] > boxMax[i])
                return -1.0f;
        } else {
            float invD = 1.0f / dir[i];
            float t1 = (boxMin[i] - origin[i]) * invD;
            float t2 = (boxMax[i] - origin[i]) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return -1.0f;
        }
    }

    return tmin;
}

#endif // HEADLESS_SERVER

// src/Interaction/EntityPicker.h
//
// Client-side ray-AABB intersection picker.
//
// Given a mouse ray (origin + direction) in world space, EntityPicker
// iterates over all entities in the registry that have a visual component
// (StaticModelComponent, ColliderComponent, or AssimpModelComponent with a
// BoundingBox) and returns the closest entity whose bounding box is
// intersected by the ray.
//
// This is the first step in "The Anatomy of a Click": translating a 2-D
// mouse position into a 3-D entity handle with a network ID.
//
// Guard: this class is compiled only when HEADLESS_SERVER is NOT defined,
//        because it depends on BoundingBox (rendering-side geometry).
//
// Usage:
//   EntityPicker picker(registry);
//   auto [hit, entity] = picker.pick(rayOrigin, rayDir);
//   if (hit != entt::null) { ... }

#ifndef ENGINE_ENTITY_PICKER_H
#define ENGINE_ENTITY_PICKER_H

#ifndef HEADLESS_SERVER

#include <entt/entt.hpp>
#include <glm/glm.hpp>

class EntityPicker {
public:
    explicit EntityPicker(entt::registry& registry);

    /// Find the closest entity whose bounding box is hit by the ray.
    ///
    /// @param rayOrigin     World-space ray origin (camera position or near-plane point).
    /// @param rayDirection  Normalised world-space ray direction.
    /// @return              The entt::entity handle of the closest hit, or
    ///                      entt::null if nothing was intersected.
    entt::entity pick(const glm::vec3& rayOrigin,
                      const glm::vec3& rayDirection) const;

private:
    entt::registry& registry_;

    /// Ray-AABB slab intersection test.
    /// @param rayOrigin     Ray origin in world space.
    /// @param rayDirInv     Component-wise inverse of the ray direction (1/dir).
    /// @param boxMin        AABB minimum corner in world space.
    /// @param boxMax        AABB maximum corner in world space.
    /// @param tHit          [out] Distance along the ray to the near intersection.
    /// @return              True if the ray intersects the box at t > 0.
    static bool rayAABB(const glm::vec3& rayOrigin,
                        const glm::vec3& rayDirInv,
                        const glm::vec3& boxMin,
                        const glm::vec3& boxMax,
                        float&           tHit);
};

#endif // !HEADLESS_SERVER
#endif // ENGINE_ENTITY_PICKER_H

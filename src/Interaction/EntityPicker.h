// src/Interaction/EntityPicker.h
//
// Client-side utility that performs Ray-AABB intersection tests against all
// renderable entities in the ECS registry to determine which entity (if any)
// the player's mouse cursor is pointing at.
//
// Usage:
//   EntityPicker picker(registry);
//   auto hit = picker.pick(rayOrigin, rayDir);
//   if (hit != entt::null) {
//       auto* nid = registry.try_get<NetworkIdComponent>(hit);
//       // send ActionRequestPacket with nid->id
//   }

#ifndef HEADLESS_SERVER

#ifndef ENGINE_ENTITY_PICKER_H
#define ENGINE_ENTITY_PICKER_H

#include <entt/entt.hpp>
#include <glm/glm.hpp>

class EntityPicker {
public:
    explicit EntityPicker(entt::registry& registry);

    /// Cast a ray through the scene and return the closest entity whose bounding
    /// box it intersects, or entt::null if nothing was hit.
    ///
    /// @param rayOrigin  World-space origin of the ray (camera position for
    ///                   primary rays from the near-plane unproject).
    /// @param rayDir     Normalised world-space direction of the ray.
    /// @return           The closest hit entity, or entt::null.
    entt::entity pick(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const;

    /// Convenience: build a pick ray from a window-space mouse position using
    /// the camera's view and projection matrices.
    ///
    /// @param mouseX     Mouse cursor X in window pixels.
    /// @param mouseY     Mouse cursor Y in window pixels.
    /// @param screenW    Framebuffer width in pixels.
    /// @param screenH    Framebuffer height in pixels.
    /// @param projection Camera projection matrix.
    /// @param view       Camera view matrix.
    /// @param outOrigin  [out] Ray origin in world space.
    /// @param outDir     [out] Normalised ray direction in world space.
    static void buildPickRay(float mouseX, float mouseY,
                             int screenW, int screenH,
                             const glm::mat4& projection, const glm::mat4& view,
                             glm::vec3& outOrigin, glm::vec3& outDir);

private:
    /// Test a ray against an axis-aligned bounding box.
    /// Returns the distance to the near intersection, or -1 if no hit.
    static float rayAABB(const glm::vec3& origin, const glm::vec3& dir,
                         const glm::vec3& boxMin, const glm::vec3& boxMax);

    entt::registry& registry_;
};

#endif // ENGINE_ENTITY_PICKER_H
#endif // HEADLESS_SERVER

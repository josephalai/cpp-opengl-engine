#ifndef ECS_TRANSFORMCOMPONENT_H
#define ECS_TRANSFORMCOMPONENT_H
#include <glm/glm.hpp>

struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    float     scale    = 1.0f;

    /// Phase 4 Step 4.2 — Server-side double-precision accessors.
    /// The server retains absolute 64-bit coordinates for large-world
    /// accuracy; clients receive 32-bit floats relative to their shifted origin.
    glm::dvec3 absolutePosition = glm::dvec3(0.0);

    /// Sync the 32-bit render position from the authoritative 64-bit position,
    /// applying an origin offset (used by the client after an origin shift).
    void syncFromAbsolute(const glm::dvec3& originOffset) {
        position = glm::vec3(absolutePosition - originOffset);
    }

    /// Update the absolute position from the current 32-bit position,
    /// adding back the accumulated origin offset.
    void syncToAbsolute(const glm::dvec3& originOffset) {
        absolutePosition = glm::dvec3(position) + originOffset;
    }
};

#endif // ECS_TRANSFORMCOMPONENT_H

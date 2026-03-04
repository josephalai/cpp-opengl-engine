// src/Culling/Frustum.h
// Represents the 6 planes of the camera view frustum for visibility testing.

#ifndef ENGINE_FRUSTUM_H
#define ENGINE_FRUSTUM_H

#include <glm/glm.hpp>

class Frustum {
public:
    /// Extract the 6 frustum planes from a combined view-projection matrix.
    /// Uses the Griggs-Hartmann method (transpose + row operations).
    void update(const glm::mat4& viewProjectionMatrix);

    /// Test an axis-aligned bounding box against the frustum.
    /// Uses the P-vertex optimisation: for each plane, only the corner
    /// farthest in the plane-normal direction is tested.
    /// Returns false if the AABB is completely outside any single plane.
    bool isAABBVisible(const glm::vec3& min, const glm::vec3& max) const;

    /// Test a sphere against the frustum.
    /// Returns false if the sphere centre is more than radius behind any plane.
    bool isSphereVisible(const glm::vec3& center, float radius) const;

private:
    // Plane order: left, right, bottom, top, near, far.
    // Each glm::vec4 stores (A, B, C, D) for the plane equation Ax+By+Cz+D=0.
    glm::vec4 planes_[6];
};

#endif // ENGINE_FRUSTUM_H

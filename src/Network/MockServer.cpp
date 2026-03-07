// src/Network/MockServer.cpp

#include "MockServer.h"

#include <glm/glm.hpp>
#include <cmath>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

MockServer& MockServer::instance() {
    static MockServer s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
// Path generators
// ---------------------------------------------------------------------------

glm::vec3 MockServer::generatePosition(float t) const {
    // Circular orbit in the XZ plane around the configured centre.
    const float angle = t * kAngularSpeed;
    return {
        kOrbitCentreX + kOrbitRadius * std::cos(angle),
        kOrbitCentreY,
        kOrbitCentreZ + kOrbitRadius * std::sin(angle)
    };
}

glm::vec3 MockServer::generateRotation(float t) const {
    // Rotate to face the direction of travel (tangent to the circle) so the
    // entity always faces forward along its path.
    const float yawDeg = glm::degrees(t * kAngularSpeed) + 90.0f;
    return { 0.0f, yawDeg, 0.0f };
}

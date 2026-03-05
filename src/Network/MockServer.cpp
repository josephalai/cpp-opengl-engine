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
// registerComponent
// ---------------------------------------------------------------------------

void MockServer::registerComponent(NetworkSyncComponent* comp) {
    if (!comp) return;
    components_.push_back(comp);

    // Prime the buffer with an initial snapshot at the starting position so
    // the component has something to hold at before the first tick fires.
    Network::TransformSnapshot initial;
    initial.sequenceNumber = sequenceNum_++;
    initial.timestamp      = serverTime_;
    initial.position       = generatePosition(serverTime_);
    initial.rotation       = generateRotation(serverTime_);
    comp->pushSnapshot(initial);
}

// ---------------------------------------------------------------------------
// update — called every render frame
// ---------------------------------------------------------------------------

void MockServer::update(float deltaTime) {
    if (components_.empty()) return;

    serverTime_       += deltaTime;
    tickAccumulator_  += deltaTime;

    if (tickAccumulator_ >= kTickInterval) {
        tickAccumulator_ -= kTickInterval;
        dispatchSnapshot();
    }
}

// ---------------------------------------------------------------------------
// dispatchSnapshot — fire one server tick
// ---------------------------------------------------------------------------

void MockServer::dispatchSnapshot() {
    Network::TransformSnapshot snap;
    snap.sequenceNumber = sequenceNum_++;
    snap.timestamp      = serverTime_;
    snap.position       = generatePosition(serverTime_);
    snap.rotation       = generateRotation(serverTime_);

    for (NetworkSyncComponent* comp : components_) {
        if (comp) {
            comp->pushSnapshot(snap);
        }
    }
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

// src/Network/MockServer.h
//
// Phase 1 — Authoritative Server Simulator.
//
// MockServer is a lightweight singleton that emulates the authoritative game
// server running locally at a fixed Simulation Tick Rate (10 Hz / 100 ms).
// Each tick it generates a new TransformSnapshot on the server timeline.
// The real ENet transport (NetworkSystem) now handles snapshot delivery;
// MockServer is retained for path-generation utilities.

#ifndef ENGINE_MOCKSERVER_H
#define ENGINE_MOCKSERVER_H

#include "NetworkPackets.h"

#include <cstdint>

class MockServer {
public:
    // Singleton access — not copyable or moveable.
    static MockServer& instance();
    MockServer(const MockServer&)            = delete;
    MockServer& operator=(const MockServer&) = delete;

    // -------------------------------------------------------------------------
    // Tuning constants
    // -------------------------------------------------------------------------

    /// Server tick interval in seconds (100 ms = 10 Hz).
    static constexpr float kTickInterval = 0.1f;

    /// Radius of the circular demo path (world units).
    static constexpr float kOrbitRadius = 20.0f;

    /// Centre of the circular orbit (matches the player's default spawn point
    /// so the network entity is always visible on startup).
    static constexpr float kOrbitCentreX = 100.0f;
    static constexpr float kOrbitCentreY =   3.0f;
    static constexpr float kOrbitCentreZ = -80.0f;

    /// Angular speed of the orbit (radians per second).
    static constexpr float kAngularSpeed = 0.8f;

    // -------------------------------------------------------------------------
    // Path generators
    // -------------------------------------------------------------------------

    /// World-space position at server time t.
    glm::vec3 generatePosition(float t) const;

    /// Euler-angle rotation (degrees) at server time t.
    glm::vec3 generateRotation(float t) const;

private:
    MockServer() = default;
};

#endif // ENGINE_MOCKSERVER_H

// src/Network/MockServer.h
//
// Phase 1 — Authoritative Server Simulator.
//
// MockServer is a lightweight singleton that emulates the authoritative game
// server running locally at a fixed Simulation Tick Rate (10 Hz / 100 ms).
// Each tick it generates a new TransformSnapshot for every registered
// NetworkSyncComponent and pushes it into that component's buffer, exactly as
// a real server would do over a UDP socket.
//
// How to use:
//   1. Create a NetworkSyncComponent and attach it to an Entity.
//   2. Call MockServer::instance().registerComponent(comp) once.
//   3. Call MockServer::instance().update(deltaTime) every frame from the
//      main loop (the NetworkSystem ISystem does this automatically).
//   The component will start receiving snapshots and interpolating.
//
// Extending the motion path:
//   Override generatePosition() / generateRotation() or replace the lambda
//   approach in update() to drive any path you need (straight line, spline,
//   waypoint list, etc.).

#ifndef ENGINE_MOCKSERVER_H
#define ENGINE_MOCKSERVER_H

#include "NetworkPackets.h"
#include "../Entities/Components/NetworkSyncComponent.h"

#include <vector>
#include <cstdint>

class MockServer {
public:
    // Singleton access — not copyable or moveable.
    static MockServer& instance();
    MockServer(const MockServer&)            = delete;
    MockServer& operator=(const MockServer&) = delete;

    // -------------------------------------------------------------------------
    // Server interface
    // -------------------------------------------------------------------------

    /// Register a NetworkSyncComponent to receive tick updates.
    /// An initial snapshot is dispatched immediately so the buffer is primed
    /// with a starting position before the first tick fires.
    void registerComponent(NetworkSyncComponent* comp);

    /// Advance the server simulation.  Call once per render frame with the
    /// real elapsed time.  Fires a tick (and dispatches snapshots to all
    /// registered components) whenever kTickInterval seconds have elapsed.
    void update(float deltaTime);

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

private:
    MockServer() = default;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    std::vector<NetworkSyncComponent*> components_;

    float    tickAccumulator_ = 0.0f;  ///< Time since last tick (seconds).
    float    serverTime_      = 0.0f;  ///< Monotonic server clock (seconds).
    uint32_t sequenceNum_     = 0;     ///< Next outgoing packet sequence number.

    // -------------------------------------------------------------------------
    // Path generators — override these to change movement behaviour.
    // -------------------------------------------------------------------------

    /// World-space position at server time t.
    glm::vec3 generatePosition(float t) const;

    /// Euler-angle rotation (degrees) at server time t.
    glm::vec3 generateRotation(float t) const;

    /// Build and dispatch a snapshot to all registered components.
    void dispatchSnapshot();
};

#endif // ENGINE_MOCKSERVER_H

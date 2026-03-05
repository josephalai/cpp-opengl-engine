// src/Server/ServerMain.cpp
//
// Phase 2 — Headless Authoritative Server.
//
// A minimal standalone executable that:
//   1. Initializes ENet and creates a server host on port 7777.
//   2. Runs a 10 Hz (100 ms) tick loop generating circular-orbit
//      TransformSnapshot packets.
//   3. Broadcasts each snapshot to all connected ENet peers.
//
// This executable does NOT use OpenGL, GLFW, or any rendering subsystem.
// It only depends on ENet, GLM (header-only), and NetworkPackets.h.

#include "../Network/NetworkPackets.h"

#include <enet/enet.h>
#include <glm/glm.hpp>

#include <iostream>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

// ---------------------------------------------------------------------------
// Configuration — matches MockServer Phase 1 constants
// ---------------------------------------------------------------------------

static constexpr int   kPort         = 7777;
static constexpr int   kMaxClients   = 32;
static constexpr int   kChannelCount = 2;

static constexpr float kTickInterval = 0.1f;   // 100 ms = 10 Hz
static constexpr float kOrbitRadius  = 20.0f;
static constexpr float kOrbitCentreX = 100.0f;
static constexpr float kOrbitCentreY =   3.0f;
static constexpr float kOrbitCentreZ = -80.0f;
static constexpr float kAngularSpeed =  0.8f;

// ---------------------------------------------------------------------------
// Graceful shutdown on SIGINT / SIGTERM
// ---------------------------------------------------------------------------

static std::atomic<bool> g_running{true};

static void signalHandler(int /*sig*/) {
    g_running.store(false);
}

// ---------------------------------------------------------------------------
// Path generators (identical to MockServer Phase 1)
// ---------------------------------------------------------------------------

static glm::vec3 generatePosition(float t) {
    const float angle = t * kAngularSpeed;
    return {
        kOrbitCentreX + kOrbitRadius * std::cos(angle),
        kOrbitCentreY,
        kOrbitCentreZ + kOrbitRadius * std::sin(angle)
    };
}

static glm::vec3 generateRotation(float t) {
    const float yawDeg = glm::degrees(t * kAngularSpeed) + 90.0f;
    return { 0.0f, yawDeg, 0.0f };
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    // Register signal handlers for graceful shutdown.
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // --- ENet Initialization ---
    if (enet_initialize() != 0) {
        std::cerr << "[Server] Failed to initialize ENet.\n";
        return 1;
    }

    // --- Create Server Host ---
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(kPort);

    ENetHost* server = enet_host_create(
        &address,       // bind address
        kMaxClients,    // max clients
        kChannelCount,  // channels
        0,              // unlimited incoming bandwidth
        0               // unlimited outgoing bandwidth
    );

    if (!server) {
        std::cerr << "[Server] Failed to create ENet server host on port "
                  << kPort << ".\n";
        enet_deinitialize();
        return 1;
    }

    std::cout << "[Server] Listening on port " << kPort
              << " (tick rate: " << static_cast<int>(1.0f / kTickInterval)
              << " Hz)\n";

    // --- Server State ---
    float    serverTime  = 0.0f;
    uint32_t sequenceNum = 0;

    using Clock = std::chrono::steady_clock;
    auto lastTick = Clock::now();

    // --- Main Loop ---
    while (g_running.load()) {
        // Process ENet events (non-blocking).
        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "[Server] Client connected from "
                              << event.peer->address.host << ":"
                              << event.peer->address.port << "\n";
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "[Server] Client disconnected.\n";
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    // Server ignores client packets for now (Phase 3 will
                    // handle client input).
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }

        // --- Tick Rate Enforcement ---
        auto now     = Clock::now();
        float elapsed = std::chrono::duration<float>(now - lastTick).count();

        if (elapsed >= kTickInterval) {
            lastTick = now;
            serverTime += kTickInterval;

            // Build snapshot.
            Network::TransformSnapshot snapshot;
            snapshot.sequenceNumber = sequenceNum++;
            snapshot.timestamp      = serverTime;
            snapshot.position       = generatePosition(serverTime);
            snapshot.rotation       = generateRotation(serverTime);

            // Serialize and broadcast to all connected peers.
            // flags=0 means unreliable delivery — acceptable for position
            // snapshots that are continuously superseded by newer data.
            ENetPacket* packet = enet_packet_create(
                &snapshot,
                sizeof(snapshot),
                0  // Unreliable: no ENET_PACKET_FLAG_RELIABLE flag.
            );

            enet_host_broadcast(server, 0, packet);
            enet_host_flush(server);
        }

        // Sleep for roughly the remaining time until the next tick to reduce
        // CPU usage.  We clamp the minimum sleep to 1 ms so we still wake up
        // in time to process ENet events between ticks.
        auto now2    = Clock::now();
        float used   = std::chrono::duration<float>(now2 - lastTick).count();
        float remain = kTickInterval - used;
        if (remain > 0.001f) {
            auto sleepMs = std::chrono::milliseconds(
                static_cast<int>(remain * 1000.0f));
            std::this_thread::sleep_for(sleepMs);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // --- Cleanup ---
    std::cout << "[Server] Shutting down...\n";
    enet_host_destroy(server);
    enet_deinitialize();

    return 0;
}

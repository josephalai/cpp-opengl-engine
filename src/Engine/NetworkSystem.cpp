// src/Engine/NetworkSystem.cpp
//
// Phase 2 — ENet client that receives TransformSnapshot packets from the
// headless server and feeds them into NetworkSyncComponent buffers.

#include "NetworkSystem.h"
#include "../Network/NetworkPackets.h"

#include <iostream>
#include <cstring>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NetworkSystem::NetworkSystem(std::vector<Entity*> netEntities)
    : netEntities_(std::move(netEntities))
{}

// ---------------------------------------------------------------------------
// init — connect to the headless server via ENet
// ---------------------------------------------------------------------------

void NetworkSystem::init() {
    // Create an ENet client host (no incoming connections, 1 outgoing).
    client_ = enet_host_create(
        nullptr,        // no bind address — this is a client
        1,              // max 1 outgoing connection (to the server)
        kChannelCount,  // number of channels
        0,              // unlimited incoming bandwidth
        0               // unlimited outgoing bandwidth
    );

    if (!client_) {
        std::cerr << "[NetworkSystem] Failed to create ENet client host.\n";
        return;
    }

    // Resolve the server address.
    ENetAddress address;
    enet_address_set_host(&address, kServerHost);
    address.port = static_cast<enet_uint16>(kServerPort);

    // Initiate the connection.  enet_host_connect returns a peer handle
    // immediately; the actual handshake completes asynchronously and is
    // reported via ENET_EVENT_TYPE_CONNECT in update().
    serverPeer_ = enet_host_connect(client_, &address, kChannelCount, 0);
    if (!serverPeer_) {
        std::cerr << "[NetworkSystem] Failed to initiate ENet connection to "
                  << kServerHost << ":" << kServerPort << ".\n";
        return;
    }

    std::cout << "[NetworkSystem] Connecting to " << kServerHost
              << ":" << kServerPort << " ...\n";
}

// ---------------------------------------------------------------------------
// update — poll ENet and push received snapshots into components
// ---------------------------------------------------------------------------

void NetworkSystem::update(float deltaTime) {
    if (!client_) return;

    ENetEvent event;

    // Process all pending ENet events (non-blocking: timeout = 0).
    while (enet_host_service(client_, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << "[NetworkSystem] Connected to server.\n";
                break;

            case ENET_EVENT_TYPE_RECEIVE: {
                // Validate packet size before casting.
                if (event.packet->dataLength == sizeof(Network::TransformSnapshot)) {
                    Network::TransformSnapshot snapshot;
                    std::memcpy(&snapshot, event.packet->data, sizeof(snapshot));

                    // Push the snapshot into every registered entity's
                    // NetworkSyncComponent (same interface as MockServer used).
                    for (Entity* e : netEntities_) {
                        if (!e) continue;
                        auto* sync = e->getComponent<NetworkSyncComponent>();
                        if (sync) {
                            sync->pushSnapshot(snapshot);
                        }
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << "[NetworkSystem] Disconnected from server.\n";
                serverPeer_ = nullptr;
                break;

            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }

    // Drive interpolation on every network-controlled entity.
    for (Entity* e : netEntities_) {
        if (e) {
            e->updateComponents(deltaTime);
        }
    }
}

// ---------------------------------------------------------------------------
// shutdown — disconnect and destroy the ENet host
// ---------------------------------------------------------------------------

void NetworkSystem::shutdown() {
    if (serverPeer_) {
        enet_peer_disconnect(serverPeer_, 0);

        // Allow up to 3 seconds for the disconnect to complete gracefully.
        ENetEvent event;
        bool disconnected = false;
        while (enet_host_service(client_, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                disconnected = true;
                break;
            }
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                enet_packet_destroy(event.packet);
            }
        }
        if (!disconnected) {
            enet_peer_reset(serverPeer_);
        }
        serverPeer_ = nullptr;
    }

    if (client_) {
        enet_host_destroy(client_);
        client_ = nullptr;
    }

    std::cout << "[NetworkSystem] Shut down.\n";
}

// ---------------------------------------------------------------------------
// addEntity
// ---------------------------------------------------------------------------

void NetworkSystem::addEntity(Entity* e) {
    if (e) {
        netEntities_.push_back(e);
    }
}

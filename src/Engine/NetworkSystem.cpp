// src/Engine/NetworkSystem.cpp
//
// Phase 5+ — Multi-Client Networking with Unified Pipeline.
//
// Each frame:
//   1. Read the Player's actual physics-driven position and send it to the
//      server as a PlayerInputPacket (which now includes position/rotation).
//   2. Poll ENet — handle Welcome, Spawn, Despawn, TransformSnapshot packets.
//   3. For the local player: apply server reconciliation to the actual Player
//      entity when the server disagrees significantly.
//   4. For remote entities: push snapshots into their NetworkSyncComponent
//      buffer for smooth interpolation.

#include "NetworkSystem.h"
#include "../Entities/Player.h"

#include <iostream>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NetworkSystem::NetworkSystem(const std::string& serverIP,
                             Player*         localPlayer,
                             SpawnCallback   onSpawn,
                             DespawnCallback onDespawn)
    : serverIP_(serverIP)
    , localPlayer_(localPlayer)
    , spawnCallback_(std::move(onSpawn))
    , despawnCallback_(std::move(onDespawn))
{}

// ---------------------------------------------------------------------------
// init — connect to the headless server via ENet
// ---------------------------------------------------------------------------

void NetworkSystem::init() {
    client_ = enet_host_create(nullptr, 1, kChannelCount, 0, 0);

    if (!client_) {
        std::cerr << "[NetworkSystem] Failed to create ENet client host.\n";
        return;
    }

    ENetAddress address;
    enet_address_set_host(&address, serverIP_.c_str());
    address.port = static_cast<enet_uint16>(kServerPort);

    serverPeer_ = enet_host_connect(client_, &address, kChannelCount, 0);
    if (!serverPeer_) {
        std::cerr << "[NetworkSystem] Failed to initiate ENet connection to "
                  << serverIP_ << ":" << kServerPort << ".\n";
        return;
    }

    std::cout << "[NetworkSystem] Connecting to " << serverIP_
              << ":" << kServerPort << " ...\n";
}

// ---------------------------------------------------------------------------
// update — send player state, poll ENet, reconcile / interpolate
// ---------------------------------------------------------------------------

void NetworkSystem::update(float deltaTime) {
    if (!client_) return;

    // -----------------------------------------------------------------
    // 1. Send the local player's actual physics-driven position to the
    //    server.  The Player entity has already been moved by the
    //    PhysicsSystem (Bullet) before this system runs.
    // -----------------------------------------------------------------
    if (localPlayer_ && serverPeer_) {
        float forward = InputMaster::isKeyDown(W) ?  1.0f
                      : InputMaster::isKeyDown(S) ? -1.0f : 0.0f;
        float turn    = InputMaster::isKeyDown(A) ?  1.0f
                      : InputMaster::isKeyDown(D) ? -1.0f : 0.0f;

        Network::PlayerInputPacket input;
        input.sequenceNumber = ++inputSequenceNumber_;
        input.forward        = forward;
        input.turn           = turn;
        input.deltaTime      = deltaTime;
        input.position       = localPlayer_->getPosition();
        input.rotation       = localPlayer_->getRotation();

        auto buf = Network::serialise(Network::PacketType::PlayerInput,
                                      input);
        ENetPacket* packet = enet_packet_create(
            buf.data(), buf.size(), 0 /* unreliable */);
        enet_peer_send(serverPeer_, 0, packet);
    }

    // -----------------------------------------------------------------
    // 2. Poll ENet for incoming events
    // -----------------------------------------------------------------
    ENetEvent event;

    while (enet_host_service(client_, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << "[NetworkSystem] Connected to server.\n";
                break;

            case ENET_EVENT_TYPE_RECEIVE: {
                if (event.packet->dataLength < 1) {
                    enet_packet_destroy(event.packet);
                    break;
                }

                auto ptype = static_cast<Network::PacketType>(
                    event.packet->data[0]);
                const uint8_t* payload = event.packet->data + 1;
                size_t plen = event.packet->dataLength - 1;

                // ----- WelcomePacket -----
                if (ptype == Network::PacketType::Welcome &&
                    plen == sizeof(Network::WelcomePacket)) {
                    Network::WelcomePacket wp;
                    std::memcpy(&wp, payload, sizeof(wp));
                    localPlayerId_ = wp.localNetworkId;
                    std::cout << "[NetworkSystem] Welcome — localPlayerId = "
                              << localPlayerId_ << "\n";

                    // Link the local Player entity to our real network id.
                    if (localPlayer_) {
                        networkEntities_[localPlayerId_] = localPlayer_;
                    }
                }

                // ----- SpawnPacket -----
                else if (ptype == Network::PacketType::Spawn &&
                         plen == sizeof(Network::SpawnPacket)) {
                    Network::SpawnPacket sp;
                    std::memcpy(&sp, payload, sizeof(sp));
                    std::cout << "[NetworkSystem] Spawn — networkId "
                              << sp.networkId << " model=\"" << sp.modelType
                              << "\" at (" << sp.position.x << ", "
                              << sp.position.y << ", " << sp.position.z
                              << ")\n";

                    // If this is us, the local Player is already linked.
                    if (sp.networkId == localPlayerId_) {
                        // Already registered via WelcomePacket handling.
                        if (networkEntities_.find(localPlayerId_) ==
                            networkEntities_.end() && localPlayer_) {
                            networkEntities_[localPlayerId_] = localPlayer_;
                        }
                    } else {
                        // Remote entity — create via callback.
                        // Guard: skip if we already have an entity for this id.
                        if (networkEntities_.find(sp.networkId) !=
                            networkEntities_.end()) {
                            std::cout << "[NetworkSystem] Spawn ignored — "
                                         "entity " << sp.networkId
                                      << " already exists.\n";
                        } else if (spawnCallback_) {
                            Entity* e = spawnCallback_(sp.networkId,
                                                       sp.modelType,
                                                       sp.position);
                            if (e) {
                                networkEntities_[sp.networkId] = e;
                            }
                        }
                    }
                }

                // ----- DespawnPacket -----
                else if (ptype == Network::PacketType::Despawn &&
                         plen == sizeof(Network::DespawnPacket)) {
                    Network::DespawnPacket dp;
                    std::memcpy(&dp, payload, sizeof(dp));
                    std::cout << "[NetworkSystem] Despawn — networkId "
                              << dp.networkId << "\n";

                    auto it = networkEntities_.find(dp.networkId);
                    if (it != networkEntities_.end()) {
                        if (despawnCallback_) {
                            despawnCallback_(dp.networkId, it->second);
                        }
                        networkEntities_.erase(it);
                    }
                }

                // ----- TransformSnapshot -----
                else if (ptype == Network::PacketType::TransformSnapshot &&
                         plen == sizeof(Network::TransformSnapshot)) {
                    Network::TransformSnapshot snapshot;
                    std::memcpy(&snapshot, payload, sizeof(snapshot));

                    if (snapshot.networkId == localPlayerId_) {
                        // === Server Reconciliation (local player) ===
                        // The physics engine is authoritative on the client.
                        // If the server disagrees significantly, snap the
                        // player to the server's position.
                        if (localPlayer_) {
                            glm::vec3 diff = snapshot.position - localPlayer_->getPosition();
                            float distSq = glm::dot(diff, diff);
                            // Only correct if desync is > 5 units (avoids jitter).
                            if (distSq > 25.0f) {
                                localPlayer_->setPosition(snapshot.position);
                                localPlayer_->setRotation(snapshot.rotation);
                                std::cout << "[NetworkSystem] Server reconciliation — "
                                             "snapped local player to server position.\n";
                            }
                        }
                    } else {
                        // === Remote entity — push into interp buffer ===
                        auto it = networkEntities_.find(snapshot.networkId);
                        if (it != networkEntities_.end() && it->second) {
                            auto* sync = it->second->getComponent<
                                NetworkSyncComponent>();
                            if (sync) {
                                sync->pushSnapshot(snapshot);
                            }
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

    // -----------------------------------------------------------------
    // 3. Drive interpolation on remote entities
    // -----------------------------------------------------------------
    for (auto& [nid, ent] : networkEntities_) {
        if (nid == localPlayerId_ || !ent) continue;
        ent->updateComponents(deltaTime);
    }
}

// ---------------------------------------------------------------------------
// shutdown — disconnect and destroy the ENet host
// ---------------------------------------------------------------------------

void NetworkSystem::shutdown() {
    if (serverPeer_) {
        enet_peer_disconnect(serverPeer_, 0);

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
// addEntity / removeEntity
// ---------------------------------------------------------------------------

void NetworkSystem::addEntity(uint32_t networkId, Entity* e) {
    if (e) {
        networkEntities_[networkId] = e;
    }
}

void NetworkSystem::removeEntity(uint32_t networkId) {
    networkEntities_.erase(networkId);
}

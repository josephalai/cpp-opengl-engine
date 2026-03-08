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
//   4. For remote entities: push snapshots into their NetworkSyncData
//      buffer for smooth interpolation.

#include "NetworkSystem.h"
#include "../Entities/Player.h"

#include <iostream>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NetworkSystem::NetworkSystem(entt::registry&  registry,
                             const std::string& serverIP,
                             Player*         localPlayer,
                             SpawnCallback   onSpawn,
                             DespawnCallback onDespawn)
    : registry_(registry)
    , serverIP_(serverIP)
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
    // 0. Apply smooth server reconciliation interpolation.
    // -----------------------------------------------------------------
    if (hasReconcileTarget_ && localPlayer_) {
        glm::vec3 cur = localPlayer_->getPosition();

        // 1. Lerp XZ horizontally (always trust the server's XZ)
        cur.x = glm::mix(cur.x, reconcileTarget_.x, kReconcileLerp);
        cur.z = glm::mix(cur.z, reconcileTarget_.z, kReconcileLerp);

        // 2. Threshold Y (Mask Bullet's collision margin discrepancies)
        float yDiff = std::abs(cur.y - reconcileTarget_.y);
        if (yDiff > 0.15f) { // Only correct Y if it's a real desync (e.g. jumping/falling)
            cur.y = glm::mix(cur.y, reconcileTarget_.y, kReconcileLerp * 0.5f); // Softer vertical lerp
        }

        localPlayer_->setPosition(cur);

        // Stop interpolating once we're close enough on the XZ plane
        glm::vec3 remaining = reconcileTarget_ - cur;
        remaining.y = 0.0f; // Ignore Y for the cancellation check
        
        if (glm::dot(remaining, remaining) < 0.01f) {
            hasReconcileTarget_ = false;
        }
    }

    // -----------------------------------------------------------------
    // 1. Send the local player's input flags to the server.
    //    [Phase 3.2] The client NEVER sends its position/rotation over
    //    the wire.  Only raw button states and the camera yaw are sent.
    //    The server independently simulates movement using SharedMovement.
    // -----------------------------------------------------------------
    if (localPlayer_ && serverPeer_) {
        Network::PlayerInputPacket input;
        input.sequenceNumber = ++inputSequenceNumber_;
        input.deltaTime      = deltaTime;
        input.cameraYaw      = localPlayer_->getRotation().y;
        input.moveForward    = InputMaster::isKeyDown(W);
        input.moveBackward   = InputMaster::isKeyDown(S);
        // [Phase 3.3] A/D are TURN keys on the client (they increment rotation.y
        // in PlayerMovementSystem). Their effect is already captured in cameraYaw.
        // Sending them as strafe flags caused the server to add a perpendicular
        // displacement that the client never applied → per-frame desync.
        input.moveLeft       = false;
        input.moveRight      = false;
        input.jump           = InputMaster::isKeyDown(Space);

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
                        // Snap the local player to the server's authoritative spawn
                        // position.  The server already terrain-clamps the Y, so this
                        // ensures both sides start at exactly the same position and
                        // prevents a cascade of reconciliation snaps while the client
                        // is still "settling" from its scene-file spawn height.
                        if (localPlayer_) {
                            localPlayer_->setPosition(sp.position);
                        }
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
                        // Only correct if the server disagrees by more than
                        // kReconcileThreshSq. Instead of a hard snap (which
                        // causes a visible teleport), record the target and
                        // smoothly LERP toward it across the next frames.
                        if (localPlayer_) {
                            glm::vec3 diff = snapshot.position - localPlayer_->getPosition();
                            
                            // Ignore tiny vertical discrepancies when deciding to reconcile
                            if (std::abs(diff.y) < 0.15f) {
                                diff.y = 0.0f; 
                            }
                            
                            float distSq = glm::dot(diff, diff);
                            if (distSq > kReconcileThreshSq) {
                                reconcileTarget_    = snapshot.position;
                                hasReconcileTarget_ = true;
                                localPlayer_->setRotation(snapshot.rotation);
                                std::cout << "[NetworkSystem] Server reconciliation triggered.\n";
                            }
                        }
                    } else {
                        // === Remote entity — push into interp buffer ===
                        auto it = networkEntities_.find(snapshot.networkId);
                        if (it != networkEntities_.end() && it->second) {
                            // Push into ECS NetworkSyncData.
                            entt::entity handle = it->second->getHandle();
                            if (auto* nsd = registry_.try_get<NetworkSyncData>(handle)) {
                                // Discard out-of-order packets.
                                if (nsd->buffer.empty() ||
                                    snapshot.sequenceNumber > nsd->buffer.back().sequenceNumber) {
                                    nsd->buffer.push_back(snapshot);

                                    // Clamp buffer to maxBufferSize.
                                    while (nsd->buffer.size() > nsd->maxBufferSize) {
                                        nsd->buffer.pop_front();
                                    }

                                    // Sync playback clock on 2nd snapshot.
                                    if (!nsd->started && nsd->buffer.size() >= 2) {
                                        nsd->renderTime = nsd->buffer.back().timestamp;
                                        nsd->started    = true;
                                    }
                                }
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

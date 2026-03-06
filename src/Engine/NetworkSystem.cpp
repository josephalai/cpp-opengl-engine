// src/Engine/NetworkSystem.cpp
//
// Phase 5+ — Multi-Client Networking.
//
// Each frame:
//   1. Capture local input, predict movement instantly, push to history buffer,
//      and send the input packet to the server (with PacketType prefix).
//   2. Poll ENet — handle Welcome, Spawn, Despawn, TransformSnapshot packets.
//   3. For the local player: snap + reconcile (replay unacknowledged inputs).
//   4. For remote entities: push snapshots into their NetworkSyncComponent
//      buffer for smooth interpolation.

#include "NetworkSystem.h"
#include "../Network/SharedMovement.h"

#include <iostream>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NetworkSystem::NetworkSystem(const std::string& serverIP,
                             SpawnCallback   onSpawn,
                             DespawnCallback onDespawn)
    : serverIP_(serverIP)
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
// update — prediction, send input, poll ENet, reconcile / interpolate
// ---------------------------------------------------------------------------

void NetworkSystem::update(float deltaTime) {
    if (!client_) return;

    // -----------------------------------------------------------------
    // 1. Capture local input and predict movement immediately
    // -----------------------------------------------------------------
    float forward = InputMaster::isKeyDown(W) ?  1.0f
                  : InputMaster::isKeyDown(S) ? -1.0f : 0.0f;
    float turn    = InputMaster::isKeyDown(A) ?  1.0f
                  : InputMaster::isKeyDown(D) ? -1.0f : 0.0f;

    if (forward != 0.0f || turn != 0.0f) {
        Network::PlayerInputPacket input;
        input.sequenceNumber = ++inputSequenceNumber_;
        input.forward        = forward;
        input.turn           = turn;
        input.deltaTime      = deltaTime;

        // Predict locally using shared movement math.
        SharedMovement::applyInput(input, predictedPosition_,
                                   predictedRotation_);

        pendingInputs_.push_back(input);

        // Send to server with PacketType prefix.
        if (serverPeer_) {
            auto buf = Network::serialise(Network::PacketType::PlayerInput,
                                          input);
            ENetPacket* packet = enet_packet_create(
                buf.data(), buf.size(), 0 /* unreliable */);
            enet_peer_send(serverPeer_, 0, packet);
        }
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

                    // Re-map the placeholder entity (id 0) to our real id.
                    auto it0 = networkEntities_.find(0);
                    if (it0 != networkEntities_.end()) {
                        networkEntities_[localPlayerId_] = it0->second;
                        networkEntities_.erase(it0);
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

                    // If this is us, link to netEntities_[localPlayerId_].
                    if (sp.networkId == localPlayerId_) {
                        // The local entity was already created by the Engine;
                        // its entry in networkEntities_ should exist from
                        // Engine::initNetworkEntity.  If not, use the
                        // callback.
                        if (networkEntities_.find(localPlayerId_) ==
                            networkEntities_.end() && spawnCallback_) {
                            Entity* e = spawnCallback_(sp.networkId,
                                                       sp.modelType,
                                                       sp.position);
                            if (e) networkEntities_[sp.networkId] = e;
                        }
                    } else {
                        // Remote entity — create via callback.
                        if (spawnCallback_) {
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

                        // a) Snap to authoritative state.
                        predictedPosition_ = snapshot.position;
                        predictedRotation_ = snapshot.rotation;

                        // b) Discard acknowledged inputs.
                        while (!pendingInputs_.empty() &&
                               pendingInputs_.front().sequenceNumber
                                   <= snapshot.lastProcessedInputSequence) {
                            pendingInputs_.pop_front();
                        }

                        // c) Replay remaining unacknowledged inputs.
                        for (const auto& inp : pendingInputs_) {
                            SharedMovement::applyInput(inp,
                                predictedPosition_, predictedRotation_);
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
    // 3. Apply predicted position to the local player entity
    // -----------------------------------------------------------------
    if (localPlayerId_ != 0) {
        auto it = networkEntities_.find(localPlayerId_);
        if (it != networkEntities_.end() && it->second) {
            it->second->setPosition(predictedPosition_);
            it->second->setRotation(predictedRotation_);
        }
    }

    // -----------------------------------------------------------------
    // 4. Drive interpolation on remote entities
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

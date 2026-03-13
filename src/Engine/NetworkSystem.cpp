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
#include "../Physics/PhysicsSystem.h"
#include "../Network/SharedMovement.h"
#include "../Events/EventBus.h"
#include "../Events/EntityClickedEvent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/InputStateComponent.h"
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
    // Subscribe to EntityClickedEvent published by InputDispatcher when the
    // player right-clicks on an interactable world entity.  We translate this
    // directly into an ActionRequestPacket sent to the server so it can start
    // the server-side interaction state machine (InteractionSystem).
    EventBus::instance().subscribe<EntityClickedEvent>([this](const EntityClickedEvent& e) {
        sendActionRequest(e.networkId);
    });

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
    // 0. Apply smooth server-authoritative reconciliation each frame.
    //    When the server has moved the player via pathfinding (or any
    //    other server-side authority) we store a target position instead
    //    of hard-snapping.  Move toward the target at exactly run speed
    //    so the animation is indistinguishable from normal WASD walking
    //    and the player never stops mid-path waiting for the next tick.
    // -----------------------------------------------------------------
    if (hasReconcileTarget_ && localPlayer_) {
        glm::vec3 curr   = localPlayer_->getPosition();
        glm::vec3 target = reconcileTarget_;
        target.y = curr.y;  // Physics owns the Y axis

        glm::vec3 diff = target - curr;
        diff.y = 0.0f;
        float distSq = glm::dot(diff, diff);

        if (distSq < 0.0025f) {   // within 0.05 m — snap and clear
            localPlayer_->setPosition(target);
            if (physicsSystem_) physicsSystem_->warpPlayer(target);
            hasReconcileTarget_ = false;

            // Stop animation — tell AnimationSystem we are no longer moving.
            // if (auto* is = registry_.try_get<InputStateComponent>(localPlayer_->getHandle())) {
            //     is->currentSpeed = 0.0f;
            // }
        } else {
            // Move at the same constant run speed used by WASD input so the
            // auto-walk animation and pace exactly match normal player movement.
            // This replaces the previous exponential LERP (kReconcileLerp = 0.3)
            // which slowed to a crawl near the target and caused a visible
            // stop-wait-restart cycle each time a new server tick arrived.
            const float runSpd  = SharedMovement::runSpeed();
            const float maxStep = runSpd * deltaTime;
            const float dist    = std::sqrt(distSq);
            const float stepFrac = (maxStep >= dist) ? 1.0f : maxStep / dist;

            glm::vec3 newPos = glm::mix(curr, target, stepFrac);
            newPos.y = curr.y;
            localPlayer_->setPosition(newPos);
            if (physicsSystem_) physicsSystem_->warpPlayer(newPos);

            // Report the actual per-frame speed so AnimationSystem plays Run.
            if (auto* is = registry_.try_get<InputStateComponent>(localPlayer_->getHandle())) {
                float moved = glm::length(newPos - curr);
                is->currentSpeed = (deltaTime > 0.0001f) ? moved / deltaTime : runSpd;
            }
        }
    }

    // -----------------------------------------------------------------
    // 1. Send the local player's input flags to the server.
    //    [Phase 3.2] The client NEVER sends its position/rotation over
    //    the wire.  Only raw button states and the camera yaw are sent.
    //    The server independently simulates movement using SharedMovement.
    // -----------------------------------------------------------------
    if (localPlayer_ && serverPeer_) {

        // Record the actual physical result of the PREVIOUS frame's input
        // so the reconciliation handler can compare it against the server
        // snapshot for the same sequence number.
        if (inputSequenceNumber_ > 0) {
            localHistory_.push_back({inputSequenceNumber_, localPlayer_->getPosition()});
            if (localHistory_.size() > kMaxLocalHistorySize) localHistory_.erase(localHistory_.begin());
        }

        Network::PlayerInputPacket input;
        input.sequenceNumber = ++inputSequenceNumber_;

        input.deltaTime      = deltaTime;
        input.cameraYaw      = localPlayer_->getRotation().y;
        input.moveForward    = InputMaster::isActionDown("MoveForward");
        input.moveBackward   = InputMaster::isActionDown("MoveBackward");
        // [Phase 3.3] A/D are TURN keys on the client (they increment rotation.y
        // in PlayerMovementSystem). Their effect is already captured in cameraYaw.
        // Sending them as strafe flags caused the server to add a perpendicular
        // displacement that the client never applied → per-frame desync.
        input.moveLeft       = false;
        input.moveRight      = false;
        input.jump           = InputMaster::isActionDown("Jump");

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
                        networkEntities_[localPlayerId_] = localPlayer_->getHandle();
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
                            hasReconcileTarget_ = false;
                            localHistory_.clear();
                            if (physicsSystem_) physicsSystem_->warpPlayer(sp.position);
                        }
                        // Clear any history recorded before the server-authoritative
                        // spawn position was applied.  Stale history entries (recorded
                        // when the client was at its scene-file position before this
                        // snap) would cause a spurious large reconciliation diff on the
                        // first TransformSnapshot, triggering an unwanted LERP walk.
                        localHistory_.clear();
                        hasReconcileTarget_ = false;

                        // Already registered via WelcomePacket handling.
                        if (networkEntities_.find(localPlayerId_) ==
                            networkEntities_.end() && localPlayer_) {
                            networkEntities_[localPlayerId_] = localPlayer_->getHandle();
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
                            entt::entity e = spawnCallback_(sp.networkId,
                                                            sp.modelType,
                                                            sp.position);
                            if (e != entt::null) {
                                networkEntities_[sp.networkId] = e;
                                // Stamp the server-assigned network ID back onto
                                // the ECS component so EntityPicker→InputDispatcher
                                // can include it in EntityClickedEvent correctly.
                                if (auto* nid = registry_.try_get<NetworkIdComponent>(e)) {
                                    nid->id = sp.networkId;
                                }
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

                // ----- ServerMessagePacket -----
                // Reliable dialogue / notification text from a server Lua script.
                // In Phase 9 this feeds into the ImGui chat box.  For now we
                // print it clearly to the client terminal as proof-of-life.
                else if (ptype == Network::PacketType::ServerMessage &&
                         plen == sizeof(Network::ServerMessagePacket)) {
                    Network::ServerMessagePacket msgPkt;
                    std::memcpy(&msgPkt, payload, sizeof(msgPkt));
                    // Guarantee null termination even if the server forgot.
                    msgPkt.message[Network::kMaxMessageLen - 1] = '\0';
                    std::cout << "\n===================================================\n";
                    std::cout << "[NPC DIALOGUE]: " << msgPkt.message << "\n";
                    std::cout << "===================================================\n\n";
                }

                // ----- TransformSnapshot -----
                // ----- TransformSnapshot -----
                else if (ptype == Network::PacketType::TransformSnapshot &&
                         plen == sizeof(Network::TransformSnapshot)) {
                    Network::TransformSnapshot snapshot;
                    std::memcpy(&snapshot, payload, sizeof(snapshot));

                    if (snapshot.networkId == localPlayerId_) {
                        if (localPlayer_) {
                            glm::vec3 currentClientPos = localPlayer_->getPosition();

                            // The server position with the client's authoritative Y.
                            glm::vec3 serverPos = glm::vec3(snapshot.position.x,
                                                             currentClientPos.y,
                                                             snapshot.position.z);

                            // ---- Already in server-authoritative LERP mode ----
                            // While lerping (e.g. mid-pathfinding), skip history-based
                            // reconciliation entirely.  Just advance the target to the
                            // latest server position and let the per-frame lerp catch up.
                            if (hasReconcileTarget_) {
                                reconcileTarget_ = serverPos;
                                localHistory_.clear();
                            } else {
                                // ---- Normal history-based reconciliation ----
                                glm::vec3 historicalPos = currentClientPos;

                                // 1. Find where the client WAS when the server processed this
                                auto it = std::find_if(localHistory_.begin(), localHistory_.end(),
                                    [&](const PlayerHistory& h) { return h.sequenceNumber == snapshot.lastProcessedInputSequence; });

                                if (it != localHistory_.end()) {
                                    historicalPos = it->position;
                                    // Clear this entry and all older ones so they cannot
                                    // re-trigger reconciliation on future snapshots.
                                    localHistory_.erase(localHistory_.begin(), std::next(it));
                                }

                                // 2. Compare Server Past vs Client Past
                                glm::vec3 diff = snapshot.position - historicalPos;
                                diff.y = 0.0f; // Exclude Y

                                float distSq = glm::dot(diff, diff);
                                if (distSq > kReconcileThreshSq) {
                                    // Determine whether the client was stationary when the
                                    // server processed this input.  If the client did not
                                    // move (clientMovedSq ≈ 0), the server position delta
                                    // is authoritative (e.g. pathfinding, knockback).
                                    // Start LERP toward the server position instead of
                                    // hard-snapping (which would cause a visible skip).
                                    glm::vec3 clientDelta = currentClientPos - historicalPos;
                                    clientDelta.y = 0.0f;
                                    float clientMovedSq = glm::dot(clientDelta, clientDelta);

                                    if (clientMovedSq <= kReconcileThreshSq) {
                                        // Server-authoritative movement: begin smooth LERP.
                                        reconcileTarget_    = serverPos;
                                        hasReconcileTarget_ = true;
                                        localHistory_.clear();
                                    } else {
                                        // 3. Genuine prediction error: apply the mathematical
                                        //    error to our CURRENT position.
                                        glm::vec3 correctedPos = currentClientPos + diff;
                                        correctedPos.y = currentClientPos.y; // Preserve client Y

                                        localPlayer_->setPosition(correctedPos);

                                        if (physicsSystem_) {
                                            physicsSystem_->warpPlayer(correctedPos);
                                        }

                                        std::cout << "[NetworkSystem] Real Reconcile Triggered.\n"
                                                  << "   -> Client Hist Pos: (" << historicalPos.x << ", " << historicalPos.z << ")\n"
                                                  << "   -> Server Snap Pos: (" << snapshot.position.x << ", " << snapshot.position.z << ")\n"
                                                  << "   -> XZ Discrepancy : (" << diff.x << ", " << diff.z << ")\n";
                                    }
                                }
                            }
                        }
                    } else {
                        // === Remote entity — push into interp buffer ===
                        auto it = networkEntities_.find(snapshot.networkId);
                        if (it != networkEntities_.end() && it->second != entt::null) {
                            // Push into ECS NetworkSyncData directly via entt::entity handle.
                            if (auto* nsd = registry_.try_get<NetworkSyncData>(it->second)) {
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
                                        // Seed renderTime so that targetTime starts at the
                                        // OLDEST snapshot in the buffer.  While !started the
                                        // entity displays buffer.front().position, so seeding
                                        // here causes no visible position jump.  More importantly,
                                        // targetTime immediately enters the interpolation window
                                        // (between front and back) on the very next frame, giving
                                        // continuous smooth movement without the starvation-driven
                                        // walk-idle oscillation that back+delay seeding causes.
                                        nsd->renderTime = nsd->buffer.front().timestamp + nsd->interpolationDelay;
                                        nsd->started = true;
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

void NetworkSystem::addEntity(uint32_t networkId, entt::entity e) {
    if (e != entt::null) {
        networkEntities_[networkId] = e;
    }
}

void NetworkSystem::removeEntity(uint32_t networkId) {
    networkEntities_.erase(networkId);
}

// ---------------------------------------------------------------------------
// sendActionRequest — send an ActionRequestPacket to the server
// ---------------------------------------------------------------------------

void NetworkSystem::sendActionRequest(uint32_t targetNetworkId) {
    if (!serverPeer_) return;

    Network::ActionRequestPacket req;
    req.targetNetworkId = targetNetworkId;
    req.action          = Network::ActionType::None; // Let the server decide from InteractableComponent

    auto buf = Network::serialise(Network::PacketType::ActionRequest, req);
    enet_peer_send(serverPeer_, 0,
        enet_packet_create(buf.data(), buf.size(),
            ENET_PACKET_FLAG_RELIABLE));
}

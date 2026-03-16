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
#include "../Entities/PlayerCamera.h"
#include "../Physics/PhysicsSystem.h"
#include "../Network/SharedMovement.h"
#include "../Events/EventBus.h"
#include "../Events/EntityClickedEvent.h"
#include "../Events/Event.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../ECS/Components/AnimatedModelComponent.h"
#include "../Guis/Chat/ChatBox.h"
#include "../Guis/Inventory/InventoryGrid.h"
#include "../Guis/Skills/SkillsPanel.h"
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

    // Subscribe to ContextMenuActionEvent (Phase 2) — when the player selects
    // an action from the OSRS right-click context menu, forward it to the server
    // as an ActionRequestPacket with the chosen action type.
    EventBus::instance().subscribe<ContextMenuActionEvent>([this](const ContextMenuActionEvent& e) {
        sendActionRequest(e.targetNetworkId);
        std::cout << "[NetworkSystem] ContextMenu action '" << e.actionName
                  << "' → ActionRequest for entity " << e.targetNetworkId << "\n";
    });

    // Subscribe to ChatReceivedEvent that originated locally (the player typed
    // something in the ChatBox) — senderNetworkId == 0 means "local outgoing".
    EventBus::instance().subscribe<ChatReceivedEvent>([this](const ChatReceivedEvent& e) {
        if (e.senderNetworkId == 0) {
            // Local outgoing message — send it to the server.
            sendChatMessage(e.message);
        }
        // Incoming messages (senderNetworkId != 0) are already displayed by
        // the ChatMessagePacket handler above; no further action needed here.
    });

    // Initialise the chat box event subscriptions.
    ChatBox::instance().init();

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

    // Tick the startup grace period so reconciliation is suppressed for
    // the first ~0.5 s after the local player's spawn position is applied.
    if (startupGracePeriod_ > 0.0f) {
        startupGracePeriod_ -= deltaTime;
        if (startupGracePeriod_ < 0.0f) startupGracePeriod_ = 0.0f;
    }

    // -----------------------------------------------------------------
    // 0. Apply smooth server-authoritative reconciliation each frame.
    //    When the server has moved the player via pathfinding (or any
    //    other server-side authority) we store a target position instead
    //    of hard-snapping.  Move toward the target at exactly run speed
    //    so the animation is indistinguishable from normal WASD walking
    //    and the player never stops mid-path waiting for the next tick.
    // -----------------------------------------------------------------
    if (hasReconcileTarget_ && localPlayer_) {
        // --- WASD override: cancel reconciliation when the player takes
        //     direct keyboard control.  Without this, PhysicsSystem advances
        //     the player in the WASD direction each frame, then this code
        //     warps the player back toward the reconcile target — creating a
        //     visible per-frame position oscillation (stutter). ---
        bool anyMoveKeyHeld = InputMaster::isActionDown("MoveForward")  ||
                              InputMaster::isActionDown("MoveBackward") ||
                              InputMaster::isActionDown("MoveLeft")     ||
                              InputMaster::isActionDown("MoveRight");
        if (anyMoveKeyHeld) {
            hasReconcileTarget_ = false;
            localHistory_.clear();

            // Re-enable delta-based animation and reset the baseline so
            // AnimationSystem does not see a stale position delta.
            {
                auto amcView = registry_.view<AnimatedModelComponent>();
                for (auto e : amcView) {
                    auto& amc = amcView.get<AnimatedModelComponent>(e);
                    if (amc.isLocalPlayer) {
                        amc.suppressDeltaAnimation = false;
                        amc.lastPosition           = localPlayer_->getPosition();
                        break;
                    }
                }
            }
        } else {
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

                // Reconciliation complete — re-enable delta-based animation and
                // reset the last-position baseline so there is no one-frame spike.
                // AMC lives on a separate render entity (isLocalPlayer=true), NOT
                // on the Player's own ECS entity, so we must search for it.
                {
                    auto amcView = registry_.view<AnimatedModelComponent>();
                    for (auto e : amcView) {
                        auto& amc = amcView.get<AnimatedModelComponent>(e);
                        if (amc.isLocalPlayer) {
                            amc.suppressDeltaAnimation = false;
                            amc.lastPosition           = target;
                            break;
                        }
                    }
                }
            } else {
                // Move at the same constant run speed used by WASD input so the
                // auto-walk animation and pace exactly match normal player movement.
                const float runSpd  = SharedMovement::runSpeed();
                const float maxStep = runSpd * deltaTime;
                const float dist    = std::sqrt(distSq);
                const float stepFrac = (maxStep >= dist) ? 1.0f : maxStep / dist;

                glm::vec3 newPos = glm::mix(curr, target, stepFrac);
                newPos.y = curr.y;
                localPlayer_->setPosition(newPos);
                if (physicsSystem_) physicsSystem_->warpPlayer(newPos);

                // Face directly toward the target — recompute yaw every frame
                // from the actual curr→target vector so the player always faces
                // where it is walking.
                if (auto* tc = registry_.try_get<TransformComponent>(localPlayer_->getHandle())) {
                    glm::vec3 toTarget = target - curr;
                    toTarget.y = 0.0f;
                    if (glm::dot(toTarget, toTarget) > 1e-4f) {
                        glm::vec3 dir = glm::normalize(toTarget);
                        tc->rotation.y = glm::degrees(std::atan2(dir.x, dir.z));
                    }
                }

                // Report the actual per-frame speed so AnimationSystem plays Run.
                if (auto* is = registry_.try_get<InputStateComponent>(localPlayer_->getHandle())) {
                    float moved = glm::length(newPos - curr);
                    is->currentSpeed = (deltaTime > 0.0001f) ? moved / deltaTime : runSpd;
                }
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
        // Camera-relative movement: use the camera's independent orbit yaw,
        // not the player model's rotation.y.  The player model's rotation is
        // now a visual snap driven by movement direction and is not the authority
        // on where the player is "looking" for purposes of movement.
        input.cameraYaw      = playerCamera_ ? playerCamera_->getOrbitYaw()
                                             : localPlayer_->getRotation().y;
        input.moveForward    = InputMaster::isActionDown("MoveForward");
        input.moveBackward   = InputMaster::isActionDown("MoveBackward");
        // A/D are now strafe keys (camera-relative).  SharedMovement::applyInput
        // uses cameraYaw as the forward reference and moveLeft/moveRight as
        // perpendicular strafe flags — so client and server stay in sync.
        input.moveLeft       = InputMaster::isActionDown("MoveLeft");
        input.moveRight      = InputMaster::isActionDown("MoveRight");
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

                            // Reset the animation delta baseline to the spawn position
                            // so the first frame has zero delta instead of a huge jump
                            // from the old (0,0,0) default.  Also clear any stale
                            // reconciliation suppression that may have been active.
                            if (auto* amc = registry_.try_get<AnimatedModelComponent>(
                                    localPlayer_->getHandle())) {
                                amc->lastPosition            = sp.position;
                                amc->lastPositionInitialized = true;
                                amc->suppressDeltaAnimation  = false;
                            }
                        }
                        // Clear any history recorded before the server-authoritative
                        // spawn position was applied.  Stale history entries (recorded
                        // when the client was at its scene-file position before this
                        // snap) would cause a spurious large reconciliation diff on the
                        // first TransformSnapshot, triggering an unwanted LERP walk.
                        localHistory_.clear();
                        hasReconcileTarget_ = false;

                        // Allow a short grace period before history-based reconciliation
                        // is active so the physics engine can settle without triggering
                        // an immediate spurious LERP walk on the first few frames.
                        startupGracePeriod_ = 0.5f;

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
                else if (ptype == Network::PacketType::ServerMessage &&
                         plen == sizeof(Network::ServerMessagePacket)) {
                    Network::ServerMessagePacket msgPkt;
                    std::memcpy(&msgPkt, payload, sizeof(msgPkt));
                    msgPkt.message[Network::kMaxMessageLen - 1] = '\0';
                    std::cout << "\n[NPC DIALOGUE]: " << msgPkt.message << "\n\n";
                    // Route to the chat box so the player sees it in-game.
                    ChatBox::instance().appendMessage("NPC", msgPkt.message);
                }

                // ----- Phase 3: ChatMessagePacket -----
                else if (ptype == Network::PacketType::ChatMessage &&
                         plen == sizeof(Network::ChatMessagePacket)) {
                    Network::ChatMessagePacket chatPkt;
                    std::memcpy(&chatPkt, payload, sizeof(chatPkt));
                    chatPkt.message[Network::kMaxChatLen - 1]      = '\0';
                    chatPkt.senderName[Network::kMaxSenderLen - 1] = '\0';
                    ChatBox::instance().appendMessage(chatPkt.senderName, chatPkt.message);
                    ChatReceivedEvent evt{};
                    evt.senderNetworkId = chatPkt.senderNetworkId;
                    evt.senderName      = chatPkt.senderName;
                    evt.message         = chatPkt.message;
                    EventBus::instance().publish(evt);
                }

                // ----- Phase 4: InventorySyncPacket -----
                else if (ptype == Network::PacketType::InventorySync &&
                         plen == sizeof(Network::InventorySyncPacket)) {
                    Network::InventorySyncPacket invPkt;
                    std::memcpy(&invPkt, payload, sizeof(invPkt));
                    InventoryGrid::instance().applySync(invPkt);
                    if (!InventoryGrid::instance().isVisible()) {
                        InventoryGrid::instance().show();
                    }
                }

                // ----- Phase 5: SkillsSyncPacket -----
                else if (ptype == Network::PacketType::SkillsSync &&
                         plen == sizeof(Network::SkillsSyncPacket)) {
                    Network::SkillsSyncPacket skPkt;
                    std::memcpy(&skPkt, payload, sizeof(skPkt));
                    SkillsPanel::instance().applySync(skPkt);
                    if (!SkillsPanel::instance().isVisible()) {
                        SkillsPanel::instance().show();
                    }
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
                            } else if (startupGracePeriod_ > 0.0f) {
                                // Still inside the post-spawn grace window — skip
                                // reconciliation so the physics can settle.
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
                                        // Compute yaw from current client position toward target
                                        // so the first step always faces directly toward the
                                        // destination, regardless of the server snapshot rotation.
                                        {
                                            glm::vec3 toTarget = serverPos - currentClientPos;
                                            toTarget.y = 0.0f;
                                            if (glm::dot(toTarget, toTarget) > 1e-4f) {
                                                glm::vec3 dir = glm::normalize(toTarget);
                                                reconcileTargetYaw_ = glm::degrees(
                                                    std::atan2(dir.x, dir.z));
                                            } else {
                                                reconcileTargetYaw_ = snapshot.rotation.y;
                                            }
                                        }
                                        hasReconcileTarget_ = true;
                                        localHistory_.clear();

                                        // Suppress delta-based animation transitions so the
                                        // walk/idle flip-flop doesn't occur while the LERP walk
                                        // carries the player toward the server target.
                                        // AMC lives on a separate render entity, not on the
                                        // Player's own entity, so search by isLocalPlayer flag.
                                        {
                                            auto amcView = registry_.view<AnimatedModelComponent>();
                                            for (auto e : amcView) {
                                                auto& amc = amcView.get<AnimatedModelComponent>(e);
                                                if (amc.isLocalPlayer) {
                                                    amc.suppressDeltaAnimation = true;
                                                    break;
                                                }
                                            }
                                        }
                                    } else {
                                        // 3. Genuine prediction error: apply the mathematical
                                        //    error to our CURRENT position.
                                        glm::vec3 correctedPos = currentClientPos + diff;
                                        correctedPos.y = currentClientPos.y; // Preserve client Y

                                        localPlayer_->setPosition(correctedPos);

                                        if (physicsSystem_) {
                                            physicsSystem_->warpPlayer(correctedPos);
                                        }

                                        // Flag the AnimatedModelComponent so AnimationSystem
                                        // knows to discard the snap-back position delta and
                                        // not let it battle the input-driven facing direction.
                                        // AMC lives on a separate render entity, not on the
                                        // Player's own entity, so search by isLocalPlayer flag.
                                        {
                                            auto amcView = registry_.view<AnimatedModelComponent>();
                                            for (auto e : amcView) {
                                                auto& amc = amcView.get<AnimatedModelComponent>(e);
                                                if (amc.isLocalPlayer) {
                                                    amc.wasSnappedBack = true;
                                                    break;
                                                }
                                            }
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

                                    // Clamp buffer to maxBufferSize.  If the buffer
                                    // overflowed (e.g. after a lag spike), drop old
                                    // snapshots and reset the interpolation clock so
                                    // the entity starts fresh from the latest data
                                    // instead of slowly replaying stale positions.
                                    if (nsd->buffer.size() > nsd->maxBufferSize) {
                                        while (nsd->buffer.size() > 3) {
                                            nsd->buffer.pop_front();
                                        }
                                        nsd->renderTime = 0.0f;
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

// ---------------------------------------------------------------------------
// Phase 3 — sendChatMessage
// ---------------------------------------------------------------------------

void NetworkSystem::sendChatMessage(const std::string& message) {
    if (!serverPeer_ || message.empty()) return;

    Network::ChatMessagePacket pkt{};
    std::strncpy(pkt.message, message.c_str(), Network::kMaxChatLen - 1);
    pkt.message[Network::kMaxChatLen - 1] = '\0';
    // senderNetworkId and senderName are filled in by the server.

    auto buf = Network::serialise(Network::PacketType::ChatMessage, pkt);
    enet_peer_send(serverPeer_, 0,
        enet_packet_create(buf.data(), buf.size(),
            ENET_PACKET_FLAG_RELIABLE));
    std::cout << "[NetworkSystem] Sent chat: " << message << "\n";
}

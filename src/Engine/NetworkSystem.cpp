// src/Engine/NetworkSystem.cpp
//
// Phase 4 — Client-Side Prediction & Server Reconciliation.
//
// Each frame:
//   1. Capture local input, predict movement instantly, push to history buffer,
//      and send the input packet to the server.
//   2. Poll ENet — when an authoritative TransformSnapshot arrives, snap to the
//      server position, discard acknowledged inputs, and replay remaining ones.

#include "NetworkSystem.h"

#include <iostream>
#include <cstring>
#include <cmath>

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
// update — prediction, send input, poll ENet, reconcile
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

        // Predict locally — apply the same math the server would.
        applyInput(input, predictedPosition_, predictedRotation_);

        // Store in history for potential replay after reconciliation.
        pendingInputs_.push_back(input);

        // Send to the server.
        if (serverPeer_) {
            ENetPacket* packet = enet_packet_create(
                &input, sizeof(input), 0 /* unreliable */);
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
                if (event.packet->dataLength == sizeof(Network::TransformSnapshot)) {
                    Network::TransformSnapshot snapshot;
                    std::memcpy(&snapshot, event.packet->data, sizeof(snapshot));

                    // --------------------------------------------------
                    // Server Reconciliation
                    // --------------------------------------------------

                    // a) Hard-set predicted state to authoritative values.
                    predictedPosition_ = snapshot.position;
                    predictedRotation_ = snapshot.rotation;

                    // b) Discard acknowledged inputs.
                    while (!pendingInputs_.empty() &&
                           pendingInputs_.front().sequenceNumber
                               <= snapshot.lastProcessedInputSequence) {
                        pendingInputs_.pop_front();
                    }

                    // c) Replay remaining unacknowledged inputs on top of
                    //    the authoritative state.
                    for (const auto& inp : pendingInputs_) {
                        applyInput(inp, predictedPosition_, predictedRotation_);
                    }

                    // Push snapshot into NetworkSyncComponent buffers
                    // (existing interpolation path for other entities).
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

    // -----------------------------------------------------------------
    // 3. Apply predicted position to the first networked entity
    // -----------------------------------------------------------------
    if (!netEntities_.empty() && netEntities_[0]) {
        netEntities_[0]->setPosition(predictedPosition_);
        netEntities_[0]->setRotation(predictedRotation_);
    }

    // Drive interpolation on remaining entities.
    for (size_t i = 1; i < netEntities_.size(); ++i) {
        if (netEntities_[i]) {
            netEntities_[i]->updateComponents(deltaTime);
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

// ---------------------------------------------------------------------------
// applyInput — shared prediction / replay math (mirrors InputComponent)
// ---------------------------------------------------------------------------

void NetworkSystem::applyInput(const Network::PlayerInputPacket& input,
                               glm::vec3& pos, glm::vec3& rot) {
    // Rotation: turn speed scaled by deltaTime (matches InputComponent).
    float turnSpeed = 0.0f;
    if      (input.turn > 0.0f) turnSpeed =  kTurnSpeed / 2.0f;
    else if (input.turn < 0.0f) turnSpeed = -kTurnSpeed / 2.0f;

    rot.y += turnSpeed * input.deltaTime;

    // Translation: forward speed scaled by deltaTime.
    float speed = 0.0f;
    if      (input.forward > 0.0f) speed =  kRunSpeed;
    else if (input.forward < 0.0f) speed = -kRunSpeed;

    float distance = speed * input.deltaTime;
    float sinY = std::sin(glm::radians(rot.y));
    float cosY = std::cos(glm::radians(rot.y));

    pos.x += distance * sinY;
    pos.z += distance * cosY;
}

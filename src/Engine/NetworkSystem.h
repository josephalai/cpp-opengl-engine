// src/Engine/NetworkSystem.h
//
// Phase 4 — Client-Side Prediction & Server Reconciliation.
//
// ISystem that drives the client-side networking:
//   1. On init(), creates an ENet client host and connects to the headless
//      server at 127.0.0.1:7777.
//   2. Each frame, captures local input, predicts movement locally, and sends
//      a PlayerInputPacket to the server while storing it in a history buffer.
//   3. When an authoritative TransformSnapshot arrives, reconciles the local
//      predicted state by snapping to the server position and replaying any
//      unacknowledged inputs from the history buffer.
//   4. On shutdown(), disconnects gracefully and destroys the ENet host.

#ifndef ENGINE_NETWORKSYSTEM_H
#define ENGINE_NETWORKSYSTEM_H

#include "ISystem.h"
#include "../Entities/Entity.h"
#include "../Entities/Components/NetworkSyncComponent.h"
#include "../Network/NetworkPackets.h"
#include "../Input/InputMaster.h"
#include <vector>
#include <deque>
#include <enet/enet.h>
#include <glm/glm.hpp>

class NetworkSystem : public ISystem {
public:
    /// Pass the initial set of network-driven entities at construction.
    explicit NetworkSystem(std::vector<Entity*> netEntities);

    void init()   override;
    void update(float deltaTime) override;
    void shutdown() override;

    /// Register an additional entity at runtime.
    void addEntity(Entity* e);

private:
    std::vector<Entity*> netEntities_;

    // --- ENet state ---
    ENetHost* client_     = nullptr;
    ENetPeer* serverPeer_ = nullptr;

    static constexpr const char* kServerHost = "127.0.0.1";
    static constexpr int         kServerPort = 7777;
    /// Number of ENet channels (channel 0 = unreliable movement data).
    static constexpr int         kChannelCount = 2;

    // --- Client-Side Prediction state ---
    std::deque<Network::PlayerInputPacket> pendingInputs_;
    glm::vec3 predictedPosition_ = {};
    glm::vec3 predictedRotation_ = {};
    uint32_t  inputSequenceNumber_ = 0;

    // --- Movement constants (must mirror InputComponent / server math) ---
    static constexpr float kRunSpeed  = 20.0f;
    static constexpr float kTurnSpeed = 160.0f;

    // --- Helpers ---
    /// Apply a single input's movement delta to the given position/rotation.
    /// This is the shared math used for both prediction and replay.
    static void applyInput(const Network::PlayerInputPacket& input,
                           glm::vec3& pos, glm::vec3& rot);
};

#endif // ENGINE_NETWORKSYSTEM_H

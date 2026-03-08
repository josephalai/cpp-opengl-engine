// src/Server/ServerMain.cpp
//
// Authoritative Headless Server — Game Engine Architecture (GEA) style.
//
// This executable does NOT use OpenGL, GLFW, or any rendering subsystem.
// It owns the master entt::registry (the ECS), runs PhysicsSystem::update()
// each tick, and broadcasts TransformComponent snapshots to all clients via ENet.
//
// Architecture
// ------------
//   Memory  — entt::registry holding TransformComponent, NetworkIdComponent, etc.
//   Physics — PhysicsSystem::update() steps the authoritative Bullet world.
//   Network — ENet receives PlayerInputPackets, sends TransformSnapshots.
//   Loop    — Fixed 10 Hz tick using std::chrono::steady_clock (no GLFW/V-sync).

#include <entt/entt.hpp>
#include "../Physics/PhysicsSystem.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/InputQueueComponent.h"
#include "../Network/NetworkPackets.h"
#include "../Network/SharedMovement.h"
#include "ServerNPCManager.h"
#include "../Terrain/HeightMap.h"
#include "../Util/FileSystem.h"
#include "../Toolbox/Maths.h"

#include <enet/enet.h>
#include <glm/glm.hpp>

#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

static constexpr int   kPort         = 7777;
static constexpr int   kMaxClients   = 32;
static constexpr int   kChannelCount = 2;
static constexpr float kTickInterval = 0.1f;   // 100 ms = 10 Hz
static constexpr float kTerrainSize  = 800.0f;

// ---------------------------------------------------------------------------
// Graceful shutdown on SIGINT / SIGTERM
// ---------------------------------------------------------------------------

static std::atomic<bool> g_running{true};

static void signalHandler(int /*sig*/) {
    g_running.store(false);
}

// ---------------------------------------------------------------------------
// Headless terrain height lookup (mirrors Terrain::getHeightOfTerrain)
// ---------------------------------------------------------------------------

struct HeadlessTerrain {
    std::vector<std::vector<float>> heights;
    float originX = 0.0f;
    float originZ = 0.0f;
    float size    = kTerrainSize;
    bool  valid   = false;

    /// Build from heightmap file for terrain tile at grid (gx, gz).
    void load(const std::string& heightmapPath, int gx, int gz) {
        Heightmap hm(heightmapPath);
        auto info = hm.getImageInfo();
        if (info.height <= 0 || info.width <= 0) {
            std::cerr << "[Server] Failed to load heightmap: "
                      << heightmapPath << "\n";
            return;
        }
        int vc = info.height;
        heights.resize(vc, std::vector<float>(vc, 0.0f));
        for (int j = 0; j < vc; ++j)
            for (int i = 0; i < vc; ++i)
                heights[j][i] = hm.getHeight(j, i);

        originX = static_cast<float>(gx) * size;
        originZ = static_cast<float>(gz) * size;
        valid = true;
        std::cout << "[Server] HeadlessTerrain loaded ("
                  << vc << "x" << vc << ") at origin ("
                  << originX << ", " << originZ << ")\n";
    }

    float getHeight(float worldX, float worldZ) const {
        if (!valid) return 0.0f;
        float tx = worldX - originX;
        float tz = worldZ - originZ;
        float gs = size / (static_cast<float>(heights.size()) - 1.0f);
        int gx = static_cast<int>(std::floor(tx / gs));
        int gz = static_cast<int>(std::floor(tz / gs));
        int maxIdx = static_cast<int>(heights.size()) - 1;
        if (gx < 0 || gz < 0 || gx >= maxIdx || gz >= maxIdx)
            return 0.0f;
        float xc = std::fmod(tx, gs) / gs;
        float zc = std::fmod(tz, gs) / gs;
        if (xc <= (1.0f - zc)) {
            return Maths::barryCentric(
                {0, heights[gx][gz], 0},
                {1, heights[gx + 1][gz], 0},
                {0, heights[gx][gz + 1], 1},
                {xc, zc});
        } else {
            return Maths::barryCentric(
                {1, heights[gx + 1][gz], 0},
                {1, heights[gx + 1][gz + 1], 1},
                {0, heights[gx][gz + 1], 1},
                {xc, zc});
        }
    }
};

// ---------------------------------------------------------------------------
// Wire-format send helpers
// ---------------------------------------------------------------------------

static void sendTo(ENetPeer* peer, Network::PacketType type,
                   const void* data, size_t sz, bool reliable = false) {
    std::vector<uint8_t> buf(1 + sz);
    buf[0] = static_cast<uint8_t>(type);
    std::memcpy(buf.data() + 1, data, sz);
    enet_peer_send(peer, 0,
        enet_packet_create(buf.data(), buf.size(),
            reliable ? ENET_PACKET_FLAG_RELIABLE : 0));
}

static void broadcast(ENetHost* host, Network::PacketType type,
                      const void* data, size_t sz, bool reliable = false) {
    std::vector<uint8_t> buf(1 + sz);
    buf[0] = static_cast<uint8_t>(type);
    std::memcpy(buf.data() + 1, data, sz);
    enet_host_broadcast(host, 0,
        enet_packet_create(buf.data(), buf.size(),
            reliable ? ENET_PACKET_FLAG_RELIABLE : 0));
}

// ---------------------------------------------------------------------------
// Helper: create a registry entity with TransformComponent + NetworkIdComponent
// ---------------------------------------------------------------------------

static entt::entity spawnEntity(entt::registry& registry,
                                 uint32_t networkId,
                                 glm::vec3 position,
                                 const std::string& modelType,
                                 bool isNPC = false) {
    auto entity = registry.create();
    registry.emplace<TransformComponent>(entity,
        TransformComponent{position, glm::vec3(0.0f), 1.0f});
    registry.emplace<NetworkIdComponent>(entity,
        NetworkIdComponent{networkId, modelType, isNPC, 0});
    // [Phase 3.3] Every entity gets an input queue so the tick loop can
    // drain it through SharedMovement::applyInput() each server tick.
    registry.emplace<InputQueueComponent>(entity);
    return entity;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // --- ENet Initialization ---
    if (enet_initialize() != 0) {
        std::cerr << "[Server] Failed to initialize ENet.\n";
        return 1;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(kPort);

    ENetHost* server = enet_host_create(&address, kMaxClients, kChannelCount, 0, 0);
    if (!server) {
        std::cerr << "[Server] Failed to create ENet server host on port "
                  << kPort << ".\n";
        enet_deinitialize();
        return 1;
    }

    std::cout << "[Server] Listening on port " << kPort
              << " (tick rate: " << static_cast<int>(1.0f / kTickInterval)
              << " Hz)\n";

    // --- Headless Terrain ---
    HeadlessTerrain terrain;
    {
        std::string hmPath = FileSystem::Texture("heightMap");
        terrain.load(hmPath, 0, -1);
    }

    // -------------------------------------------------------------------------
    // ECS Registry — the master data store for all server entities.
    // PhysicsSystem operates directly on TransformComponent via this registry.
    // -------------------------------------------------------------------------
    entt::registry registry;

    // -------------------------------------------------------------------------
    // Physics System — authoritative Bullet simulation.
    // Bound to the registry so update() reads/writes TransformComponent directly.
    // -------------------------------------------------------------------------
    PhysicsSystem physicsSystem;
    physicsSystem.setRegistry(registry);
    physicsSystem.init();
    physicsSystem.addGroundPlane(0.0f);

    // -------------------------------------------------------------------------
    // Network tracking maps
    //   peerToNetworkId    — peer → network ID (for input routing on receive)
    //   networkIdToEntity  — network ID → entt entity (for entity lookup)
    // -------------------------------------------------------------------------
    std::unordered_map<ENetPeer*, uint32_t>       peerToNetworkId;
    std::unordered_map<uint32_t, entt::entity>    networkIdToEntity;
    uint32_t nextNetworkId = 100;

    // --- NPC Loading ---
    // Prefer npcs.json (JSON format); fall back to server_npcs.cfg (legacy).
    ServerNPCManager npcManager;
    {
        std::string jsonPath = FileSystem::Scene("npcs.json");
        std::string cfgPath  = FileSystem::Scene("server_npcs.cfg");

        std::vector<NPCDefinition> defs;
        {
            std::ifstream probe(jsonPath);
            if (probe.is_open()) {
                probe.close();
                defs = npcManager.loadFromJson(jsonPath);
            } else {
                std::cout << "[Server] npcs.json not found — falling back to server_npcs.cfg\n";
                defs = npcManager.loadConfig(cfgPath);
            }
        }

        for (auto& d : defs) {
            uint32_t nid = nextNetworkId++;
            glm::vec3 pos = d.startPos;
            if (terrain.valid)
                pos.y = terrain.getHeight(pos.x, pos.z);

            auto entity = spawnEntity(registry, nid, pos, d.modelType, /*isNPC=*/true);
            networkIdToEntity[nid] = entity;
            npcManager.registerNPC(nid, d.scriptType);

            std::cout << "[Server] NPC " << nid << " (" << d.modelType
                      << ") spawned at (" << pos.x << ", " << pos.y << ", "
                      << pos.z << ") — " << d.scriptType << "\n";
        }
    }

    // --- Server Tick State ---
    float    serverTime  = 0.0f;
    uint32_t sequenceNum = 0;

    using Clock = std::chrono::steady_clock;
    auto lastTick = Clock::now();

    // === Main Loop ===
    while (g_running.load()) {
        // --- Process ENet events (non-blocking) ---
        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {
            switch (event.type) {

            // =============================================================
            // CONNECT — create registry entity, send Welcome, exchange Spawns
            // =============================================================
            case ENET_EVENT_TYPE_CONNECT: {
                uint32_t newId = nextNetworkId++;
                peerToNetworkId[event.peer] = newId;

                glm::vec3 spawnPos(100.0f, 3.0f, -80.0f);
                if (terrain.valid)
                    spawnPos.y = terrain.getHeight(spawnPos.x, spawnPos.z);

                auto entity = spawnEntity(registry, newId, spawnPos, "player");
                networkIdToEntity[newId] = entity;

                std::cout << "[Server] Client connected — networkId " << newId
                          << " from " << event.peer->address.host << ":"
                          << event.peer->address.port << "\n";

                // 1) Send WelcomePacket to the new peer.
                Network::WelcomePacket welcome;
                welcome.localNetworkId = newId;
                sendTo(event.peer, Network::PacketType::Welcome,
                       &welcome, sizeof(welcome), true);

                // 2) Send SpawnPacket for every existing entity to the new peer.
                auto view = registry.view<TransformComponent, NetworkIdComponent>();
                for (auto e : view) {
                    auto& tc  = view.get<TransformComponent>(e);
                    auto& nid = view.get<NetworkIdComponent>(e);
                    Network::SpawnPacket sp;
                    sp.networkId = nid.id;
                    sp.position  = tc.position;
                    std::strncpy(sp.modelType, nid.modelType.c_str(),
                                 Network::kModelTypeLen - 1);
                    sp.modelType[Network::kModelTypeLen - 1] = '\0';
                    sendTo(event.peer, Network::PacketType::Spawn,
                           &sp, sizeof(sp), true);
                }

                // 3) Broadcast SpawnPacket for the new player to existing peers.
                Network::SpawnPacket newSp;
                newSp.networkId = newId;
                newSp.position  = spawnPos;
                std::strncpy(newSp.modelType, "player",
                             Network::kModelTypeLen - 1);
                newSp.modelType[Network::kModelTypeLen - 1] = '\0';
                for (auto& [peer, pid] : peerToNetworkId) {
                    if (peer != event.peer) {
                        sendTo(peer, Network::PacketType::Spawn,
                               &newSp, sizeof(newSp), true);
                    }
                }
                break;
            }

            // =============================================================
            // DISCONNECT — destroy registry entity, broadcast Despawn
            // =============================================================
            case ENET_EVENT_TYPE_DISCONNECT: {
                auto it = peerToNetworkId.find(event.peer);
                if (it != peerToNetworkId.end()) {
                    uint32_t removedId = it->second;
                    peerToNetworkId.erase(it);

                    auto eit = networkIdToEntity.find(removedId);
                    if (eit != networkIdToEntity.end()) {
                        registry.destroy(eit->second);
                        networkIdToEntity.erase(eit);
                    }

                    std::cout << "[Server] Client disconnected — networkId "
                              << removedId << "\n";

                    Network::DespawnPacket dp;
                    dp.networkId = removedId;
                    broadcast(server, Network::PacketType::Despawn,
                              &dp, sizeof(dp), true);
                } else {
                    std::cout << "[Server] Unknown peer disconnected.\n";
                }
                break;
            }

            // =============================================================
            // RECEIVE — buffer PlayerInputPacket into entity's input queue
            // [Phase 3.3] The server no longer reads or trusts client
            // coordinates.  Inputs are queued here and processed
            // authoritatively by SharedMovement::applyInput() in the tick.
            // =============================================================
            case ENET_EVENT_TYPE_RECEIVE: {
                if (event.packet->dataLength >= 1) {
                    auto ptype = static_cast<Network::PacketType>(
                        event.packet->data[0]);
                    const uint8_t* payload = event.packet->data + 1;
                    size_t plen = event.packet->dataLength - 1;

                    if (ptype == Network::PacketType::PlayerInput &&
                        plen == sizeof(Network::PlayerInputPacket)) {

                        Network::PlayerInputPacket input;
                        std::memcpy(&input, payload, sizeof(input));

                        auto pit = peerToNetworkId.find(event.peer);
                        if (pit != peerToNetworkId.end()) {
                            uint32_t nid = pit->second;
                            auto eit = networkIdToEntity.find(nid);
                            if (eit != networkIdToEntity.end()) {
                                auto entity = eit->second;
                                // Append to the entity's input queue; the
                                // tick loop drains it with SharedMovement.
                                auto& queue = registry.get<InputQueueComponent>(entity);
                                queue.inputs.push_back(input);
                            }
                        }

                        // [Phase 3.3] Removed: old position-based speed-check anti-cheat.
                        // Client no longer sends coordinates, making the distance
                        // validation (distSq <= maxDist*maxDist) obsolete.
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }

        // --- Tick Rate Enforcement ---
        auto now = Clock::now();
        float elapsed = std::chrono::duration<float>(now - lastTick).count();

        if (elapsed >= kTickInterval) {
            lastTick = now;
            serverTime += kTickInterval;

            // ----- NPC AI tick — generate synthetic inputs -----
            std::unordered_map<uint32_t, Network::PlayerInputPacket> npcInputs;
            npcManager.tick(kTickInterval, npcInputs);
            for (auto& [nid, inp] : npcInputs) {
                auto eit = networkIdToEntity.find(nid);
                if (eit != networkIdToEntity.end()) {
                    auto& queue = registry.get<InputQueueComponent>(eit->second);
                    queue.inputs.push_back(inp);
                }
            }

            // ----- [Phase 3.3] Drain all entity input queues authoritatively -----
            // For every entity that received inputs since the last tick, feed
            // each queued input through SharedMovement::applyInput() to produce
            // the server's authoritative position.  Client coordinates are never
            // read — only button states + cameraYaw arrive over the wire.
            {
                auto inputView = registry.view<TransformComponent,
                                               NetworkIdComponent,
                                               InputQueueComponent>();
                for (auto entity : inputView) {
                    auto& tc      = inputView.get<TransformComponent>(entity);
                    auto& nidComp = inputView.get<NetworkIdComponent>(entity);
                    auto& queue   = inputView.get<InputQueueComponent>(entity);

                    for (const auto& inp : queue.inputs) {
                        float th = terrain.valid
                            ? terrain.getHeight(tc.position.x, tc.position.z)
                            : SharedMovement::kNoTerrainHeight;
                        SharedMovement::applyInput(inp, tc.position, tc.rotation,
                                                   queue.upwardsSpeed, queue.isInAir,
                                                   th);

                        if (inp.sequenceNumber > nidComp.lastInputSeq)
                            nidComp.lastInputSeq = inp.sequenceNumber;
                    }
                    queue.inputs.clear();
                }
            }

            // ----- Step authoritative physics simulation -----
            // PhysicsSystem reads kinematic TransformComponents → updates Bullet,
            // then writes back dynamic body positions to TransformComponents.
            physicsSystem.update(kTickInterval);

            // ----- Broadcast all entity snapshots from the registry -----
            auto view = registry.view<TransformComponent, NetworkIdComponent>();
            for (auto entity : view) {
                auto& tc  = view.get<TransformComponent>(entity);
                auto& nid = view.get<NetworkIdComponent>(entity);

                Network::TransformSnapshot snap;
                snap.networkId      = nid.id;
                snap.sequenceNumber = sequenceNum++;
                snap.timestamp      = serverTime;
                snap.position       = tc.position;
                snap.rotation       = tc.rotation;
                snap.lastProcessedInputSequence = nid.lastInputSeq;

                broadcast(server, Network::PacketType::TransformSnapshot,
                          &snap, sizeof(snap));
            }

            enet_host_flush(server);
        }

        // --- Sleep until next tick ---
        auto now2    = Clock::now();
        float used   = std::chrono::duration<float>(now2 - lastTick).count();
        float remain = kTickInterval - used;
        if (remain > 0.001f) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(remain * 1000.0f)));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // --- Cleanup ---
    std::cout << "[Server] Shutting down...\n";
    physicsSystem.shutdown();
    enet_host_destroy(server);
    enet_deinitialize();

    return 0;
}

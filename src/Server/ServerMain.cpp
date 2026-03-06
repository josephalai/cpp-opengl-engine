// src/Server/ServerMain.cpp
//
// Phases 5–8 — Headless Authoritative Server.
//
// This executable does NOT use OpenGL, GLFW, or any rendering subsystem.
// It only depends on ENet, GLM (header-only), stb_image (for heightmap),
// NetworkPackets.h, SharedMovement.h, and ServerNPCManager.
//
// Key responsibilities:
//   • Network Entity Registry — every moving object gets a unique networkId.
//   • Connection Handshake    — Welcome / Spawn / Despawn protocol.
//   • Authoritative Simulation— player inputs processed via SharedMovement.
//   • Headless Terrain        — heightmap-based Y-clamping without GL.
//   • NPC AI                  — data-driven NPCs from server_npcs.cfg.
//   • 10 Hz Broadcast         — per-entity TransformSnapshot to all clients.

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

    // --- Entity State ---
    std::unordered_map<ENetPeer*, uint32_t>          peerToId;
    std::unordered_map<uint32_t, ServerEntityState>  entities;
    uint32_t nextNetworkId = 100;

    // --- NPC Loading ---
    ServerNPCManager npcManager;
    {
        std::string npcCfg = FileSystem::Scene("server_npcs.cfg");
        auto defs = npcManager.loadConfig(npcCfg);
        for (auto& d : defs) {
            uint32_t nid = nextNetworkId++;
            ServerEntityState st;
            st.position  = d.startPos;
            st.modelType = d.modelType;
            st.isNPC     = true;
            // Clamp initial height
            if (terrain.valid) {
                st.position.y = terrain.getHeight(st.position.x, st.position.z);
            }
            entities[nid] = st;
            npcManager.registerNPC(nid, d.scriptType);
            std::cout << "[Server] NPC " << nid << " (" << d.modelType
                      << ") spawned at (" << st.position.x << ", "
                      << st.position.y << ", " << st.position.z << ") — "
                      << d.scriptType << "\n";
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
            // CONNECT — assign networkId, send Welcome, exchange Spawns
            // =============================================================
            case ENET_EVENT_TYPE_CONNECT: {
                uint32_t newId = nextNetworkId++;
                peerToId[event.peer] = newId;

                ServerEntityState st;
                st.position  = glm::vec3(100.0f, 3.0f, -80.0f);
                st.modelType = "player";
                if (terrain.valid)
                    st.position.y = terrain.getHeight(st.position.x,
                                                      st.position.z);
                entities[newId] = st;

                std::cout << "[Server] Client connected — networkId " << newId
                          << " from " << event.peer->address.host << ":"
                          << event.peer->address.port << "\n";

                // 1) Send WelcomePacket to the new peer.
                Network::WelcomePacket welcome;
                welcome.localNetworkId = newId;
                sendTo(event.peer, Network::PacketType::Welcome,
                       &welcome, sizeof(welcome), true);

                // 2) Send SpawnPacket for every existing entity to the new peer.
                for (auto& [eid, est] : entities) {
                    Network::SpawnPacket sp;
                    sp.networkId = eid;
                    sp.position  = est.position;
                    std::strncpy(sp.modelType, est.modelType.c_str(),
                                 Network::kModelTypeLen - 1);
                    sp.modelType[Network::kModelTypeLen - 1] = '\0';
                    std::cout << "[NetTrace][Server] Sending SpawnPacket to new peer"
                                 " — networkId=" << eid
                              << " modelType=\"" << sp.modelType << "\"\n";
                    sendTo(event.peer, Network::PacketType::Spawn,
                           &sp, sizeof(sp), true);
                }

                // 3) Broadcast SpawnPacket for the *new* player to all others.
                Network::SpawnPacket newSp;
                newSp.networkId = newId;
                newSp.position  = st.position;
                std::strncpy(newSp.modelType, st.modelType.c_str(),
                             Network::kModelTypeLen - 1);
                newSp.modelType[Network::kModelTypeLen - 1] = '\0';
                for (auto& [peer, pid] : peerToId) {
                    if (peer != event.peer) {
                        std::cout << "[NetTrace][Server] Broadcasting SpawnPacket"
                                     " — networkId=" << newId
                                  << " modelType=\"" << newSp.modelType << "\"\n";
                        sendTo(peer, Network::PacketType::Spawn,
                               &newSp, sizeof(newSp), true);
                    }
                }
                break;
            }

            // =============================================================
            // DISCONNECT — remove peer, broadcast Despawn
            // =============================================================
            case ENET_EVENT_TYPE_DISCONNECT: {
                auto it = peerToId.find(event.peer);
                if (it != peerToId.end()) {
                    uint32_t removedId = it->second;
                    peerToId.erase(it);
                    entities.erase(removedId);

                    std::cout << "[Server] Client disconnected — networkId "
                              << removedId << "\n";

                    // Broadcast DespawnPacket to remaining clients.
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
            // RECEIVE — process PlayerInputPacket
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

                        // Find the sender's networkId.
                        auto pit = peerToId.find(event.peer);
                        if (pit != peerToId.end()) {
                            uint32_t nid = pit->second;
                            auto eit = entities.find(nid);
                            if (eit != entities.end()) {
                                auto& est = eit->second;

                                // [NetTrace] Log input (throttled: every 60 packets)
                                if (input.sequenceNumber % 60 == 1) {
                                    std::cout << "[NetTrace][Server] PlayerInputPacket"
                                                 " networkId=" << nid
                                              << " seq=" << input.sequenceNumber
                                              << " pos=(" << input.position.x
                                              << ", " << input.position.y
                                              << ", " << input.position.z << ")\n";
                                }

                                // ------------------------------------------------
                                // Client-authoritative, server-validated model.
                                // Accept the client's physics-driven position
                                // directly (with basic speed validation).
                                // ------------------------------------------------
                                glm::vec3 delta = input.position - est.position;
                                float distSq = glm::dot(delta, delta);
                                // Scale max allowed distance by deltaTime so
                                // clients at any frame rate are treated fairly.
                                // kMaxSpeed is in units/second.
                                static constexpr float kMaxSpeed = 200.0f;
                                float maxDist = kMaxSpeed * std::max(input.deltaTime, 0.001f);
                                if (distSq <= maxDist * maxDist) {
                                    est.position = input.position;
                                    est.rotation = input.rotation;
                                } else {
                                    // Reject the move — use SharedMovement as
                                    // a fallback to keep the entity moving.
                                    float fallbackTh = terrain.valid
                                        ? terrain.getHeight(est.position.x,
                                                            est.position.z)
                                        : SharedMovement::kNoTerrainHeight;
                                    SharedMovement::applyInput(
                                        input, est.position, est.rotation, fallbackTh);
                                }

                                // Terrain height clamping (single lookup).
                                if (terrain.valid) {
                                    est.position.y = terrain.getHeight(
                                        est.position.x, est.position.z);
                                }

                                if (input.sequenceNumber >
                                    est.lastProcessedInputSequence) {
                                    est.lastProcessedInputSequence =
                                        input.sequenceNumber;
                                }
                            }
                        }
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

            // ----- NPC AI tick -----
            std::unordered_map<uint32_t, Network::PlayerInputPacket> npcInputs;
            npcManager.tick(kTickInterval, npcInputs);
            for (auto& [nid, inp] : npcInputs) {
                auto eit = entities.find(nid);
                if (eit != entities.end()) {
                    auto& est = eit->second;
                    float th = terrain.valid
                        ? terrain.getHeight(est.position.x, est.position.z)
                        : SharedMovement::kNoTerrainHeight;
                    SharedMovement::applyInput(inp, est.position,
                                               est.rotation, th);
                }
            }

            // ----- Broadcast all entity snapshots -----
            static constexpr uint32_t kBroadcastLogInterval = 100; // log every ~10 seconds
            for (auto& [eid, est] : entities) {
                Network::TransformSnapshot snap;
                snap.networkId     = eid;
                snap.sequenceNumber = sequenceNum++;
                snap.timestamp      = serverTime;
                snap.position       = est.position;
                snap.rotation       = est.rotation;
                snap.lastProcessedInputSequence = est.lastProcessedInputSequence;

                // [NetTrace] Log broadcast (throttled: every kBroadcastLogInterval sequences)
                if (snap.sequenceNumber % kBroadcastLogInterval == 0) {
                    std::cout << "[NetTrace][Server] Broadcasting TransformSnapshot"
                                 " networkId=" << eid
                              << " seq=" << snap.sequenceNumber
                              << " pos=(" << snap.position.x
                              << ", " << snap.position.y
                              << ", " << snap.position.z << ")\n";
                }

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
    enet_host_destroy(server);
    enet_deinitialize();

    return 0;
}

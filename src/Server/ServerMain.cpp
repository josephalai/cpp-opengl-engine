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
#include "../Config/ConfigManager.h"
#include "../Config/PrefabManager.h"
#include "../Config/EntityFactory.h"

#include <nlohmann/json.hpp>

#include <enet/enet.h>
#include <glm/glm.hpp>

#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <random>

// ---------------------------------------------------------------------------
// Configuration — all values are loaded from world_config.json via
// ConfigManager at the start of main().
// ---------------------------------------------------------------------------
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
    float size    = 800.0f;  // overridden at load time from ConfigManager
    bool  valid   = false;

    /// Build from heightmap file for terrain tile at grid (gx, gz).
    void load(const std::string& heightmapPath, int gx, int gz) {
        size = ConfigManager::get().physics.terrainSize;
        Heightmap hm(heightmapPath);
        auto info = hm.getImageInfo();
        if (info.height <= 0 || info.width <= 0) {
            std::cerr << "[Server] FATAL: Failed to load heightmap — tried absolute path: "
                      << heightmapPath
                      << "\n  Trees/stalls will be spawned at Y=0 and may be underground.\n";
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
// Multi-tile terrain manager — holds all loaded HeadlessTerrain tiles.
// ---------------------------------------------------------------------------

struct HeadlessTerrainManager {
    std::vector<HeadlessTerrain> tiles;

    void loadTile(const std::string& heightmapPath, int gx, int gz) {
        tiles.emplace_back();
        tiles.back().load(heightmapPath, gx, gz);
        if (!tiles.back().valid) {
            std::cerr << "[Server] WARNING: Skipping terrain tile (" << gx << ", " << gz
                      << ") — heightmap could not be loaded from: " << heightmapPath << "\n";
            tiles.pop_back();
        }
    }

    bool isAnyValid() const {
        return !tiles.empty();
    }

    float getHeight(float worldX, float worldZ) const {
        for (const auto& tile : tiles) {
            // Check if the world position falls within this tile's footprint.
            if (worldX >= tile.originX && worldX < tile.originX + tile.size &&
                worldZ >= tile.originZ && worldZ < tile.originZ + tile.size) {
                return tile.getHeight(worldX, worldZ);
            }
        }
        // Fall back to the first tile's nearest-edge value, or 0 if no tiles loaded.
        return tiles.empty() ? 0.0f : tiles[0].getHeight(worldX, worldZ);
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
// Headless scene loader — Step 2 of Phase 3 Physics.
//
// Parses scene.json WITHOUT touching OpenGL.  Only the "physics_bodies",
// "entities", and "random" arrays are read; all rendering/texture/model
// fields are ignored.  Static Bullet colliders are spawned for every scene
// entity whose alias appears in "physics_bodies".
//
// The "random" array is processed with the same pseudo-random number sequence
// as SceneLoaderJson (seeded via "random_seed" in scene.json, defaulting to 1)
// so that randomly placed trees on the server occupy exactly the same world
// positions as on every client.
// ---------------------------------------------------------------------------

static void loadHeadlessScene(entt::registry& registry,
                               PhysicsSystem& physics,
                               const HeadlessTerrainManager& terrain,
                               const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[Server] ERROR: scene.json not found — tried absolute path: "
                  << jsonPath
                  << "\n  The physics world will be empty (no trees/stalls/lamps).\n";
        return;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const std::exception& e) {
        std::cerr << "[Server] Failed to parse " << jsonPath
                  << ": " << e.what() << "\n";
        return;
    }

    // ---- Parse physics_bodies by alias ------------------------------------
    struct PhysCfg {
        std::string   type        = "static";  // "static" | "dynamic"
        std::string   shape       = "box";     // "box" | "sphere" | "capsule"
        glm::vec3     halfExtents = glm::vec3(0.5f);
        float         mass        = 1.0f;
        float         friction    = 0.5f;
        float         restitution = 0.3f;
        float         radius      = 0.5f;
        float         height      = 1.8f;
    };
    std::unordered_map<std::string, PhysCfg> physBodies;

    if (root.contains("physics_bodies") && root["physics_bodies"].is_array()) {
        for (auto& pb : root["physics_bodies"]) {
            std::string alias = pb.value("alias", "");
            if (alias.empty()) continue;
            PhysCfg cfg;
            cfg.type        = pb.value("type",        "static");
            cfg.shape       = pb.value("shape",       "box");
            cfg.friction    = pb.value("friction",    0.5f);
            cfg.restitution = pb.value("restitution", 0.3f);
            cfg.mass        = pb.value("mass",        1.0f);
            cfg.radius      = pb.value("radius",      0.5f);
            cfg.height      = pb.value("height",      1.8f);
            if (pb.contains("halfExtents") && pb["halfExtents"].is_array()
                    && pb["halfExtents"].size() >= 3) {
                auto& he = pb["halfExtents"];
                cfg.halfExtents = glm::vec3(he[0].get<float>(),
                                            he[1].get<float>(),
                                            he[2].get<float>());
            }
            physBodies[alias] = cfg;
        }
    }

    // ---- Y-value parser (mirrors SceneLoaderJson::parseJsonY) -------------
    auto parseY = [&](const nlohmann::json& entry, float x, float z) -> float {
        if (!entry.contains("y")) return 0.0f;
        auto& yv = entry["y"];
        if (yv.is_number()) return yv.get<float>();
        if (yv.is_string()) {
            std::string s = yv.get<std::string>();
            float offset = 0.0f;
            if (s.rfind("terrain", 0) == 0) {
                std::string rest = s.substr(7);
                if (!rest.empty()) {
                    try { offset = std::stof(rest); } catch (...) {}
                }
                return terrain.isAnyValid() ? terrain.getHeight(x, z) + offset : offset;
            }
            try { return std::stof(s); } catch (...) {}
        }
        return 0.0f;
    };

    // ---- Helper: spawn one scene entity with a Bullet body ----------------
    int staticCount = 0, dynamicCount = 0;
    auto spawnSceneBody = [&](const std::string& alias,
                               float x, float y, float z, float ry,
                               float scale = 1.0f) {
        auto it = physBodies.find(alias);
        if (it == physBodies.end()) return;
        const auto& cfg = it->second;

        auto entity = registry.create();
        registry.emplace<TransformComponent>(entity,
            TransformComponent{glm::vec3(x, y, z), glm::vec3(0.0f, ry, 0.0f), scale});

        ColliderShape colShape = ColliderShape::Box;
        if (cfg.shape == "sphere")  colShape = ColliderShape::Sphere;
        if (cfg.shape == "capsule") colShape = ColliderShape::Capsule;

        // Scale all dimensions so the Bullet shape matches the rendered model.
        glm::vec3 scaledHalfExtents = cfg.halfExtents * scale;
        float     scaledRadius      = cfg.radius      * scale;
        float     scaledHeight      = cfg.height      * scale;

        if (cfg.type == "dynamic") {
            PhysicsBodyDef def;
            def.type        = BodyType::Dynamic;
            def.shape       = colShape;
            def.mass        = cfg.mass;
            def.halfExtents = scaledHalfExtents;
            def.radius      = scaledRadius;
            def.height      = scaledHeight;
            def.friction    = cfg.friction;
            def.restitution = cfg.restitution;
            physics.addDynamicBody(entity, def);
            ++dynamicCount;
        } else {
            PhysicsBodyDef def;
            def.type        = BodyType::Static;
            def.shape       = colShape;
            def.mass        = 0.0f;
            def.halfExtents = scaledHalfExtents;
            def.radius      = scaledRadius;
            def.height      = scaledHeight;
            def.friction    = cfg.friction;
            def.restitution = cfg.restitution;
            physics.addDynamicBody(entity, def);
            ++staticCount;
        }
    };

    // ---- Fixed-position entities ------------------------------------------
    if (root.contains("entities") && root["entities"].is_array()) {
        for (auto& e : root["entities"]) {
            std::string alias = e.value("alias", "");
            float x     = e.value("x",     0.0f);
            float z     = e.value("z",     0.0f);
            float ry    = e.value("ry",    0.0f);
            float scale = e.value("scale", 1.0f);
            float y     = parseY(e, x, z);
            spawnSceneBody(alias, x, y, z, ry, scale);
        }
    }

    // ---- Randomly-placed entities -----------------------------------------
    // Mirror SceneLoaderJson's random placement exactly (same PRNG sequence).
    // Both sides use std::mt19937 seeded with random_seed and consume draws in
    // the same order: x, z, ry, scale [, atlas] — guaranteeing bit-identical
    // positions regardless of any other rand() calls elsewhere in the process.
    if (root.contains("random") && root["random"].is_array()) {
        unsigned int seed = root.value("random_seed", 1u);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        auto randF = [&]() { return dist01(rng); };
        std::cout << "[Server] Random seed for entity placement: " << seed << "\n";

        for (auto& r : root["random"]) {
            std::string alias    = r.value("alias",    "");
            int         count    = r.value("count",    0);
            float       scaleMin = r.value("scaleMin", 0.75f);
            float       scaleMax = r.value("scaleMax", 1.5f);
            bool        useAtlas = r.value("atlas",    false);
            bool        hasPhys  = physBodies.count(alias) > 0;

            for (int i = 0; i < count; ++i) {
                // draw 1: x  draw 2: z  draw 3: ry  draw 4: scale  [draw 5: atlas]
                float rx = randF();
                float rz = randF();
                float rr = randF();
                float rs = randF();
                if (useAtlas) randF(); // atlas index — consume to stay in sync

                if (hasPhys) {
                    float x  = std::floor(rx * 1500.f - 800.f);
                    float z  = std::floor(rz * -800.f);
                    float y  = terrain.isAnyValid() ? terrain.getHeight(x, z) : 0.0f;
                    float ry = (rr * 100.f - 50.f) * 180.0f;
                    // Mirror gRandomScale(): ceil-multiplier then clamp.
                    float multiplier = (scaleMax > 1.0f) ? std::ceil(scaleMax) : 1.0f;
                    float scale = rs * multiplier;
                    if (scale < scaleMin) scale = scaleMin;
                    if (scale > scaleMax) scale = scaleMax;
                    spawnSceneBody(alias, x, y, z, ry, scale);
                }
            }
        }
    }

    std::cout << "[Server] Headless scene loaded from " << jsonPath
              << ": " << staticCount << " static + "
              << dynamicCount << " dynamic physics bodies.\n";
    if (staticCount == 0) {
        std::cerr << "[Server] WARNING: 0 static bodies were spawned from " << jsonPath
                  << ". Players will walk through all scene geometry.\n"
                     "  Check that physics_bodies aliases match entity/random aliases in scene.json.\n";
    }
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

    // --- Data-driven initialisation ---
    ConfigManager::get().loadAll(HOME_PATH);
    PrefabManager::get().loadAll(HOME_PATH);
    const auto& cfg = ConfigManager::get();

    // --- ENet Initialization ---
    if (enet_initialize() != 0) {
        std::cerr << "[Server] Failed to initialize ENet.\n";
        return 1;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(cfg.server.port);

    ENetHost* server = enet_host_create(&address, cfg.server.maxClients,
                                         cfg.server.channelCount, 0, 0);
    if (!server) {
        std::cerr << "[Server] Failed to create ENet server host on port "
                  << cfg.server.port << ".\n";
        enet_deinitialize();
        return 1;
    }

    std::cout << "[Server] Listening on port " << cfg.server.port
              << " (tick rate: " << static_cast<int>(1.0f / cfg.server.tickInterval)
              << " Hz)\n";

    // --- Headless Terrain — load all tiles from scene.json ---
    HeadlessTerrainManager terrainMgr;
    {
        std::string scenePath = FileSystem::Scene("scene.json");
        std::string hmPath    = FileSystem::Texture("heightMap"); // default fallback

        std::ifstream sceneFile(scenePath);
        if (sceneFile.is_open()) {
            try {
                nlohmann::json sceneRoot;
                sceneFile >> sceneRoot;

                // Read heightmap filename from scene.json (default: "heightMap").
                if (sceneRoot.contains("terrain") && sceneRoot["terrain"].contains("heightmap"))
                    hmPath = FileSystem::Texture(sceneRoot["terrain"]["heightmap"].get<std::string>());

                // Load every tile listed in terrain_tiles[].
                if (sceneRoot.contains("terrain_tiles") && sceneRoot["terrain_tiles"].is_array()) {
                    for (auto& tile : sceneRoot["terrain_tiles"]) {
                        int gx = tile.value("gridX", 0);
                        int gz = tile.value("gridZ", -1);
                        terrainMgr.loadTile(hmPath, gx, gz);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[Server] Failed to parse scene.json for terrain tiles: "
                          << e.what() << "\n";
            }
        } else {
            std::cerr << "[Server] scene.json not found; falling back to single tile (0,-1)\n";
        }

        // Fallback: if no tiles were loaded from scene.json, load the primary tile.
        if (!terrainMgr.isAnyValid()) {
            terrainMgr.loadTile(hmPath, 0, -1);
        }
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
    // Add a heightfield terrain collider for every loaded tile so Bullet's
    // character controllers walk on the real terrain surface instead of Y=0.
    for (const auto& tile : terrainMgr.tiles) {
        physicsSystem.addHeadlessTerrainCollider(
            tile.heights, tile.size, tile.originX, tile.originZ);
    }

    // -------------------------------------------------------------------------
    // Headless Scene — spawn static Bullet colliders for trees, stalls, lamps.
    // Mirrors the client's scene.json parsing without any OpenGL dependency.
    // -------------------------------------------------------------------------
    {
        std::string scenePath = FileSystem::Scene("scene.json");
        loadHeadlessScene(registry, physicsSystem, terrainMgr, scenePath);
    }

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
            // TransformComponent.position is the feet position.  addCharacterController
            // internally offsets the ghost by capsuleHalfHeight, so we must NOT add it
            // here — doing so would place the ghost (and the character) 1.4 m above the
            // terrain surface, causing an initial bounce before gravity corrects it.
            if (terrainMgr.isAnyValid())
                pos.y = terrainMgr.getHeight(pos.x, pos.z);

            auto entity = spawnEntity(registry, nid, pos, d.modelType, /*isNPC=*/true);
            networkIdToEntity[nid] = entity;

            // Read capsule dimensions from the prefab if available; fall back to
            // ConfigManager defaults.  This eliminates the hardcoded 0.5f / 1.8f.
            float capsuleRadius = ConfigManager::get().physics.defaultCapsuleRadius;
            float capsuleHeight = ConfigManager::get().physics.defaultCapsuleHeight;
            if (!d.prefab.empty() && PrefabManager::get().hasPrefab(d.prefab)) {
                const auto& prefab = PrefabManager::get().getPrefab(d.prefab);
                if (prefab.contains("physics")) {
                    capsuleRadius = prefab["physics"].value("radius", capsuleRadius);
                    capsuleHeight = prefab["physics"].value("height", capsuleHeight);
                }
            }
            physicsSystem.addCharacterController(entity, capsuleRadius, capsuleHeight);
            npcManager.registerNPC(nid, d.scriptType);

            std::cout << "[Server] NPC " << nid << " (" << d.modelType
                      << ") spawned at (" << pos.x << ", " << pos.y << ", "
                      << pos.z << ") — " << d.scriptType << "\n";
        }

        // Initialise Lua scripting engine and load AI scripts from prefab
        // definitions.  Falls back to C++ AI if Lua is not available.
        npcManager.initLua(HOME_PATH);
    }

    // --- Server Tick State ---
    float    serverTime  = 0.0f;
    uint32_t sequenceNum = 0;

    std::unordered_map<uint32_t, bool> lastJumpState;

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

                glm::vec3 spawnPos = ConfigManager::get().physics.defaultSpawnPosition;
                // TransformComponent.position is the feet position.  addCharacterController
                // internally offsets the ghost by capsuleHalfHeight, so we must NOT add it
                // here — doing so would place the capsule 2×capsuleHalfHeight above terrain.
                if (terrainMgr.isAnyValid())
                    spawnPos.y = terrainMgr.getHeight(spawnPos.x, spawnPos.z);

                // Spawn player using the universal prefab factory
                auto entity = EntityFactory::spawn(registry, "player", spawnPos, &physicsSystem);
                networkIdToEntity[newId] = entity;

                // Assign the network ID to the entity (EntityFactory sets the
                // default from the prefab; override with the actual network ID).
                if (auto* nid = registry.try_get<NetworkIdComponent>(entity)) {
                    nid->id = newId;
                }

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
                        physicsSystem.removeCharacterController(eit->second);
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

        if (elapsed >= cfg.server.tickInterval) {
            lastTick = now;
            serverTime += cfg.server.tickInterval;

            // --- NEW: 5-Second Player Position Logging ---
            static float logTimer = 0.0f;
            logTimer += cfg.server.tickInterval;
            if (logTimer >= 5.0f) {
                logTimer = 0.0f;
                std::cout << "\n--- [Server] Player Positions (5s Tick) ---\n";
                auto view = registry.view<TransformComponent, NetworkIdComponent>();
                for (auto entity : view) {
                    auto& tc = view.get<TransformComponent>(entity);
                    auto& nid = view.get<NetworkIdComponent>(entity);
                    if (!nid.isNPC) {
                        std::cout << "Client " << nid.id << " at position ("
                                  << tc.position.x << ", " << tc.position.y << ", " << tc.position.z << ")\n";
                    }
                }
                std::cout << "-------------------------------------------\n";
            }

            // ----- NPC AI tick — generate synthetic inputs -----
            std::unordered_map<uint32_t, Network::PlayerInputPacket> npcInputs;
            npcManager.tick(cfg.server.tickInterval, npcInputs);
            for (auto& [nid, inp] : npcInputs) {
                auto eit = networkIdToEntity.find(nid);
                if (eit != networkIdToEntity.end()) {
                    auto& queue = registry.get<InputQueueComponent>(eit->second);
                    queue.inputs.push_back(inp);
                }
            }

            // ----- [Phase 3.3 / Physics] Drain entity input queues -----
            // Fix 3: Per-input physics stepping.
            //
            // Previously: accumulate all N inputs per entity into one displacement
            // vector → ONE 100ms Bullet step.  This caused wall-sliding and corner
            // collisions to be resolved very differently on the server (one big 100ms
            // sweep) vs. the client (N small ~16ms sweeps).
            //
            // Now: transpose the entity × input loops. For each input slot i across
            // all entities, apply that input to every entity that has one, then step
            // Bullet by the corresponding deltaTime slice. Bullet resolves each small
            // step incrementally, matching the client's per-frame behaviour.
            //
            // Fix 4: SharedMovement is now horizontal-only; Bullet's built-in
            // gravity handles Y via world gravity. Jumps are triggered directly on
            // the character controller when input.jump is true.
            {
                auto inputView = registry.view<TransformComponent,
                                               NetworkIdComponent,
                                               InputQueueComponent>();

                // Safety clamp: prevent character controllers from falling through the
                // terrain mesh if Bullet's collision resolution ever positions a capsule
                // slightly below the surface (can happen on steep slopes or at seams
                // between triangle quads).
                //
                // IMPORTANT — use a tolerance of ~5 cm before triggering a warp.
                // The btHeightfieldTerrainShape and HeadlessTerrain::getHeight use
                // identical height data and the same bilinear interpolation, so their
                // terrain heights agree to within floating-point precision.  If we warp
                // on a sub-mm difference Bullet has just corrected, we put the ghost back
                // into a position Bullet is about to move it away from, creating an
                // oscillation ("bouncing") whose amplitude grows on steep terrain where
                // the height gradient is larger.  The 0.05 m threshold ensures we only
                // intervene when the character has genuinely fallen through geometry.
                static constexpr float kTerrainClampEpsilon = 0.05f;
                auto clampCharsToTerrain = [&]() {
                    if (!terrainMgr.isAnyValid()) return;
                    auto tv = registry.view<TransformComponent, NetworkIdComponent>();
                    for (auto entity : tv) {
                        auto& tc = tv.get<TransformComponent>(entity);
                        float terrH = terrainMgr.getHeight(tc.position.x, tc.position.z);
                        
                        if (physicsSystem.hasCharacterController(entity)) {
                            // Bullet entities (NPCs)
                            if (tc.position.y < terrH - kTerrainClampEpsilon) {
                                tc.position.y = terrH;
                                physicsSystem.warpCharacterController(entity, tc.position);
                            }
                        } else {
                            // Math entities (The Player)
                            // Snap directly to the terrain to mimic the client's manual math gravity
                            tc.position.y = terrH;
                        }
                    }
                };

                // Find the maximum queue depth so we know how many sub-steps to run.
                int maxDepth = 0;
                for (auto entity : inputView) {
                    int qs = static_cast<int>(
                        inputView.get<InputQueueComponent>(entity).inputs.size());
                    maxDepth = std::max(maxDepth, qs);
                }

                if (maxDepth > 0) {
                    float subDt = cfg.server.tickInterval / static_cast<float>(maxDepth);

                    for (int step = 0; step < maxDepth; ++step) {
                        for (auto entity : inputView) {
                            auto& tc      = inputView.get<TransformComponent>(entity);
                            auto& nidComp = inputView.get<NetworkIdComponent>(entity);
                            auto& queue   = inputView.get<InputQueueComponent>(entity);

                            if (step >= static_cast<int>(queue.inputs.size())) {
                                // Prevent infinite sliding when the input queue runs dry
                                if (physicsSystem.hasCharacterController(entity)) {
                                    physicsSystem.setEntityWalkDirection(entity, glm::vec3(0.0f));
                                }
                                continue;
                            }
                            const auto& inp = queue.inputs[step];

                            if (inp.sequenceNumber > nidComp.lastInputSeq)
                                nidComp.lastInputSeq = inp.sequenceNumber;

                            if (physicsSystem.hasCharacterController(entity)) {
                                // Compute horizontal displacement for this single input.
                                glm::vec3 prevPos = tc.position;
                                SharedMovement::applyInput(inp, tc.position, tc.rotation);
                                glm::vec3 disp = tc.position - prevPos;
                                tc.position = prevPos; // revert; ghost sync writes authoritative pos

                                // Only XZ displacement passed; Bullet owns vertical.
                                physicsSystem.setEntityWalkDirection(entity,
                                    glm::vec3(disp.x, 0.0f, disp.z));

                                // Trigger a jump impulse when requested and grounded.
                                if (inp.jump && !lastJumpState[nidComp.id]) {
                                    physicsSystem.jumpCharacterController(entity);
                                }
                                lastJumpState[nidComp.id] = inp.jump;
                            } else {
                                // Fallback (no character controller): apply horizontal only.
                                SharedMovement::applyInput(inp, tc.position, tc.rotation);
                            }
                        }

                        // Step Bullet forward by one input-sized slice.
                        physicsSystem.update(subDt);

                        // Terrain-clamp Y: snap any character below the surface back up.
                        clampCharsToTerrain();
                    }

                    // Clear all queues now that every input has been consumed.
                    for (auto entity : inputView) {
                        inputView.get<InputQueueComponent>(entity).inputs.clear();
                    }
                } else {

                    // No inputs this tick — clear velocities to prevent sliding
                    for (auto entity : inputView) {
                        if (physicsSystem.hasCharacterController(entity)) {
                            physicsSystem.setEntityWalkDirection(entity, glm::vec3(0.0f));
                            lastJumpState[inputView.get<NetworkIdComponent>(entity).id] = false;
                        }
                    }
                    // No inputs this tick — still advance the simulation so gravity,
                    // dynamic bodies, and standing collision remain active.
                    physicsSystem.update(cfg.server.tickInterval);

                    // Same terrain-clamp for the no-input path.
                    clampCharsToTerrain();
                }
            }

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
        float remain = cfg.server.tickInterval - used;
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

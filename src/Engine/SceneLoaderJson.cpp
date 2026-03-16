// src/Engine/SceneLoaderJson.cpp
//
// JSON-based scene loader (Phase 1, Step 3 — Strict Data-Driven Design).
//
// Reads scene.json and populates the engine's scene vectors using the same
// code paths as SceneLoader.cpp (GPU upload, terrain, physics resolution),
// but driven entirely by JSON data instead of the legacy .cfg text parser.
//
// StringId hashing (Phase 1, Step 1) is used for modelMap lookups.
// ComponentPool (Phase 1, Step 2) is used automatically via Entity::addComponent.

#include "SceneLoaderJson.h"
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/AnimatedModelComponent.h"
#include "../ECS/Components/StaticModelComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/ColliderComponent.h"
#include "../Config/PrefabManager.h"
#include "../Entities/Entity.h"
#include "../Util/FileSystem.h"
#include "../RenderEngine/DisplayManager.h"
#include "../Util/LightUtil.h"
#include "../Textures/TerrainTexture.h"
#include "../Interaction/InteractiveModel.h"
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"
#include "../Toolbox/Color.h"
#include "../Animation/AnimationLoader.h"
#include "../BoundingBox/BoundingBox.h"
#include "StringId.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <random>

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

float gRandomFloat() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}
float gRandomScale(float mn, float mx) {
    float multiplier = (mx > 1.0f) ? std::ceil(mx) : 1.0f;
    float r = gRandomFloat() * multiplier;
    if (r < mn) r = mn;
    if (r > mx) r = mx;
    return r;
}
glm::vec3 gRandomPosition(Terrain* terrain, float yOffset = 0.0f) {
    glm::vec3 p;
    p.x = std::floor(gRandomFloat() * 1500.f - 800.f);
    p.z = std::floor(gRandomFloat() * -800.f);
    p.y = terrain ? terrain->getHeightOfTerrain(p.x, p.z) + yOffset : yOffset;
    return p;
}
glm::vec3 gRandomRotation() {
    float ry = (gRandomFloat() * 100.f - 50.f) * 180.0f;
    return glm::vec3(0.0f, ry, 0.0f);
}

// Deterministic static entity ID — mirrors ServerMain::generateStaticId exactly.
// Both client and server use this to produce matching IDs for the same world object.
inline uint32_t generateStaticId(float x, float z) {
    uint32_t ux = static_cast<uint32_t>(std::round(x * 10.0f) + 2000000.0f) & 0x7FFF;
    uint32_t uz = static_cast<uint32_t>(std::round(z * 10.0f) + 2000000.0f) & 0x7FFF;
    return 0x80000000u | (ux << 15) | uz;
}

/// Resolve a static ID collision: increment (keeping the high bit set) until
/// a free slot is found, then mark it as used.  Mirrors ServerMain::resolveStaticId.
inline uint32_t resolveStaticId(uint32_t candidate,
                                std::unordered_set<uint32_t>& usedIds,
                                float x, float z) {
    if (usedIds.count(candidate)) {
        uint32_t original = candidate;
        do {
            candidate = 0x80000000u | (((candidate & 0x7FFFFFFFu) + 1u) & 0x7FFFFFFFu);
        } while (usedIds.count(candidate));
        std::cerr << "[SceneLoaderJson] WARNING: Static ID collision at ("
                  << x << ", " << z << ") — 0x" << std::hex << original
                  << " -> 0x" << candidate << std::dec << "\n";
    }
    usedIds.insert(candidate);
    return candidate;
}

// Parse a JSON y-value: either a float or a string like "terrain" / "terrain+3".
float parseJsonY(const nlohmann::json& val, float& yOffset) {
    yOffset = 0.0f;
    if (val.is_number()) return val.get<float>();
    if (val.is_string()) {
        std::string s = val.get<std::string>();
        if (s.rfind("terrain", 0) == 0) {
            std::string rest = s.substr(7);
            if (!rest.empty()) {
                try { yOffset = std::stof(rest); } catch (...) {}
            }
            return SceneLoader::kSnapY;
        }
        try { return std::stof(s); } catch (...) {}
    }
    return 0.0f;
}

} // anonymous namespace

// ============================================================================
// SceneLoaderJson::load
// ============================================================================

bool SceneLoaderJson::load(
    const std::string&             jsonPath,
    Loader*                        loader,
    entt::registry&                registry,
    std::vector<Light*>&           lights,
    std::vector<Terrain*>&         allTerrains,
    std::vector<GuiTexture*>&      guis,
    std::vector<GUIText*>&         texts,
    std::vector<WaterTile>&        waterTiles,
    Terrain*&                      primaryTerrain,
    Player*&                       player,
    PlayerCamera*&                 playerCamera,
    std::vector<PhysicsBodyCfg>&   physicsBodyCfgs,
    std::vector<PhysicsGroundCfg>& physicsGroundCfgs)
{
    auto bytes = FileSystem::readAllBytes(jsonPath);
    if (bytes.empty()) {
        std::cerr << "[SceneLoaderJson] Cannot open: " << jsonPath << "\n";
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(bytes.begin(), bytes.end());
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[SceneLoaderJson] JSON parse error: " << e.what() << "\n";
        return false;
    }

    std::cout << "[SceneLoaderJson] Loading scene from: " << jsonPath << "\n";

    // -----------------------------------------------------------------------
    // Terrain
    // -----------------------------------------------------------------------
    std::string heightmapFile = "heightMap";
    std::string blendmapFile  = "blendMap";
    if (root.contains("terrain") && root["terrain"].is_object()) {
        auto& t = root["terrain"];
        if (t.contains("heightmap")) heightmapFile = t["heightmap"].get<std::string>();
        if (t.contains("blendmap"))  blendmapFile  = t["blendmap"].get<std::string>();
    }

    auto backgroundTex = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/grass")->getId());
    auto rTex          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/dirt")->getId());
    auto gTex          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blueflowers")->getId());
    auto bTex          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/brickroad")->getId());
    auto texturePack   = new TerrainTexturePack(backgroundTex, rTex, gTex, bTex);
    auto blendMap      = new TerrainTexture(
        loader->loadTexture("MultiTextureTerrain/" + blendmapFile)->getId());

    bool hasPrimary = false;
    if (root.contains("terrain_tiles") && root["terrain_tiles"].is_array()) {
        for (auto& tile : root["terrain_tiles"]) {
            int gx = tile.value("gridX", 0);
            int gz = tile.value("gridZ", -1);
            bool prim = tile.value("primary", false);
            auto* t = new Terrain(gx, gz, loader, texturePack, blendMap, heightmapFile);
            allTerrains.push_back(t);
            if (prim) { primaryTerrain = t; hasPrimary = true; }
        }
    }
    if (!allTerrains.empty() && !hasPrimary) {
        primaryTerrain = allTerrains[0];
    }

    // -----------------------------------------------------------------------
    // Lights
    // -----------------------------------------------------------------------
    if (root.contains("lights") && root["lights"].is_array()) {
        for (auto& l : root["lights"]) {
            std::string type = l.value("type", "directional");
            float x = l.value("x", 0.0f);
            float cr = l.value("r", 0.0f);
            float cg = l.value("g", 0.0f);
            float cb = l.value("b", 0.0f);
            float yOffset = 0.0f;
            float y = l.contains("y") ? parseJsonY(l["y"], yOffset) : 0.0f;
            bool snapY = (y == SceneLoader::kSnapY);
            float z = l.value("z", 0.0f);
            float yVal = snapY && primaryTerrain
                ? primaryTerrain->getHeightOfTerrain(x, z) + yOffset : y;

            if (type == "directional") {
                lights.push_back(new Light(
                    glm::vec3(x, yVal, z), glm::vec3(cr, cg, cb),
                    Lighting{
                        .ambient  = glm::vec3(0.2f, 0.2f, 0.2f),
                        .diffuse  = glm::vec3(0.5f, 0.5f, 0.5f),
                        .constant = Light::kDirectional,
                    }));
            } else {
                float dist = l.value("dist", 65.0f);
                auto d = LightUtil::AttenuationDistance(static_cast<int>(dist));
                Lighting li{ glm::vec3(0.2f), glm::vec3(0.5f), d.x, d.y, d.z };
                lights.push_back(new Light(glm::vec3(x, yVal, z), glm::vec3(cr, cg, cb), li));
            }
        }
    }

    // -----------------------------------------------------------------------
    // Models (parallel load + GPU upload)
    // -----------------------------------------------------------------------
    struct LoadedModel {
        TexturedModel*  model = nullptr;
        RawBoundingBox* bbox  = nullptr;
    };
    std::unordered_map<StringId, LoadedModel> modelMap;

    struct ModelEntry {
        std::string alias, objFile, textureFile;
        bool  transparent  = false;
        bool  fakeLighting = false;
        int   atlasRows    = 1;
        float shininess    = 1.0f;
        float reflectivity = 0.5f;
    };
    std::vector<ModelEntry> modelEntries;

    if (root.contains("models") && root["models"].is_array()) {
        for (auto& m : root["models"]) {
            ModelEntry me;
            me.alias       = m.value("alias", "");
            me.objFile     = m.value("obj", "");
            me.textureFile = m.value("texture", "");
            me.transparent  = m.value("transparent", false);
            me.fakeLighting = m.value("fakeLighting", false);
            me.atlasRows    = m.value("atlas", 1);
            me.shininess    = m.value("shininess", 1.0f);
            me.reflectivity = m.value("reflectivity", 0.5f);
            if (!me.alias.empty() && !me.objFile.empty())
                modelEntries.push_back(me);
        }
    }

    struct RawLoad { ModelData data; BoundingBoxData bbData; };
    std::vector<RawLoad> rawLoads(modelEntries.size());
    {
        auto f = [](ModelData* pData, BoundingBoxData* pBb, const std::string& file) {
            *pData = OBJLoader::loadObjModel(file);
            *pBb   = OBJLoader::loadBoundingBox(*pData, ClickBoxTypes::BOX, BoundTypes::AABB);
        };
        std::vector<std::thread> threads;
        for (size_t i = 0; i < modelEntries.size(); ++i)
            threads.emplace_back(f, &rawLoads[i].data, &rawLoads[i].bbData,
                                 modelEntries[i].objFile);
        for (auto& t : threads) t.join();
    }
    for (size_t i = 0; i < modelEntries.size(); ++i) {
        const auto& me = modelEntries[i];
        LoadedModel lm;
        lm.bbox = loader->loadToVAO(rawLoads[i].bbData);
        ModelTexture* tex;
        if (me.transparent || me.fakeLighting) {
            tex = new ModelTexture(me.textureFile, PNG, me.transparent, me.fakeLighting,
                                   Material{ me.shininess, me.reflectivity });
        } else {
            tex = new ModelTexture(me.textureFile, PNG,
                                   Material{ me.shininess, me.reflectivity });
        }
        if (me.atlasRows > 1) tex->setNumberOfRows(me.atlasRows);
        lm.model = new TexturedModel(loader->loadToVAO(rawLoads[i].data), tex);
        modelMap[StringId(me.alias)] = lm;

        // Cache the full mesh AABB for the editor tile footprint.
        // The OBJLoader stores Z with an inverted convention (vMin.z tracks
        // the most-positive Z value, vMax.z tracks the most-negative), and
        // the Z sentinel initialisation is also inverted, so getMin().z and
        // getMax().z always contain FLT_MAX / -FLT_MAX respectively.  We
        // therefore compute the XZ extents directly from the raw vertex buffer
        // (which is correctly interleaved as [x, y, z, x, y, z, …]) and only
        // use getMin().y / getMax().y (which work correctly) for the Y axis.
        {
            const auto& verts = rawLoads[i].data.getVertices();
            const size_t nVerts = verts.size() / 3;
            if (nVerts > 0) {
                float xMin = verts[0], xMax = verts[0];
                float zMin = verts[2], zMax = verts[2];
                for (size_t vi = 0; vi < nVerts; ++vi) {
                    const float x = verts[vi * 3];
                    const float z = verts[vi * 3 + 2];
                    if (x < xMin) xMin = x;
                    if (x > xMax) xMax = x;
                    if (z < zMin) zMin = z;
                    if (z > zMax) zMax = z;
                }
                if (xMin != xMax || zMin != zMax) {
                    const glm::vec3 meshMin(xMin, rawLoads[i].data.getMin().y, zMin);
                    const glm::vec3 meshMax(xMax, rawLoads[i].data.getMax().y, zMax);
                    PrefabManager::get().setMeshAABB(me.alias, meshMin, meshMax);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Fixed entities  →  ECS StaticModelComponent + TransformComponent
    // No Entity* objects are created; physics body setup uses entt::entity handles.
    // -----------------------------------------------------------------------
    std::unordered_map<StringId, std::vector<entt::entity>> entityAliasByHandle;

    if (root.contains("entities") && root["entities"].is_array()) {
        for (auto& e : root["entities"]) {
            std::string alias = e.value("alias", "");
            StringId aliasId(alias);
            auto it = modelMap.find(aliasId);
            if (it == modelMap.end()) {
                std::cerr << "[SceneLoaderJson] entity references unknown alias '"
                          << alias << "'\n";
                continue;
            }
            auto& lm = it->second;
            float x = e.value("x", 0.0f), z = e.value("z", 0.0f);
            float yOff = 0.0f;
            float y = e.contains("y") ? parseJsonY(e["y"], yOff) : 0.0f;
            float yVal = (y == SceneLoader::kSnapY && primaryTerrain)
                ? primaryTerrain->getHeightOfTerrain(x, z) : y;
            float rx = e.value("rx", 0.0f), ry = e.value("ry", 0.0f), rz = e.value("rz", 0.0f);
            float sc = e.value("scale", 1.0f);

            entt::entity ent = registry.create();
            auto& tc = registry.emplace<TransformComponent>(ent);
            tc.position = glm::vec3(x, yVal, z);
            tc.rotation = glm::vec3(rx, ry, rz);
            tc.scale    = sc;

            auto& smc       = registry.emplace<StaticModelComponent>(ent);
            smc.model       = lm.model;
            smc.boundingBox = new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId());
            smc.textureIndex = e.value("textureIndex", 0);

            entityAliasByHandle[aliasId].push_back(ent);
        }
    }

    // -----------------------------------------------------------------------
    // Random entities — spawned using a deterministic mt19937 PRNG so that
    // every client produces exactly the same world positions as the server
    // (ServerMain::loadHeadlessScene uses the same seed and draw order).
    //
    // Each entity with a "physics" block in its prefab also receives:
    //   - NetworkIdComponent  (deterministic ID = resolveStaticId(generateStaticId(x,z)))
    //   - ColliderComponent   (AABB from prefab physics halfExtents)
    // These are required for EntityPicker to detect right-clicks on trees,
    // and for InputDispatcher/NetworkSystem to send the correct network ID
    // in an ActionRequestPacket.
    //
    // A usedIds set is shared across random and editor_entities sections so
    // that collision resolution matches the server's order.
    // -----------------------------------------------------------------------
    std::unordered_set<uint32_t> staticUsedIds;

    if (root.contains("random") && root["random"].is_array()) {
        unsigned int seed = root.value("random_seed", 1u);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        auto randF = [&]() { return dist01(rng); };

        for (auto& r : root["random"]) {
            std::string alias    = r.value("alias",    "");
            int         count    = r.value("count",    0);
            float       scaleMin = r.value("scaleMin", 0.75f);
            float       scaleMax = r.value("scaleMax", 1.5f);
            bool        useAtlas = r.value("atlas",    false);

            StringId aliasId(alias);
            auto it = modelMap.find(aliasId);

            // Resolve prefab physics halfExtents for ColliderComponent (EntityPicker).
            glm::vec3 physHalfExtents(0.5f);
            bool      hasPrefabPhys = false;
            const auto& prefab = PrefabManager::get().getPrefab(alias);
            if (!prefab.is_null() && prefab.contains("physics")) {
                const auto& phys = prefab["physics"];
                if (phys.contains("halfExtents") && phys["halfExtents"].is_array()
                        && phys["halfExtents"].size() >= 3) {
                    physHalfExtents = glm::vec3(
                        phys["halfExtents"][0].get<float>(),
                        phys["halfExtents"][1].get<float>(),
                        phys["halfExtents"][2].get<float>());
                    hasPrefabPhys = true;
                } else if (phys.contains("radius")) {
                    float r2 = phys.value("radius", 0.5f);
                    physHalfExtents = glm::vec3(r2);
                    hasPrefabPhys = true;
                }
            }

            for (int i = 0; i < count; ++i) {
                // Consume draws in identical order to ServerMain and AssetBaker:
                // draw 1: x  draw 2: z  draw 3: ry  draw 4: scale  [draw 5: atlas]
                float rx = randF();
                float rz = randF();
                float rr = randF();
                float rs = randF();
                if (useAtlas) randF(); // atlas index — consume to stay in sync

                float x  = std::floor(rx * 1500.f - 800.f);
                float z  = std::floor(rz * -800.f);
                float y  = primaryTerrain
                    ? primaryTerrain->getHeightOfTerrain(x, z) : 0.0f;
                float ry = (rr * 100.f - 50.f) * 180.0f;

                // Mirror gRandomScale() algorithm for bit-identical results.
                float multiplier = (scaleMax > 1.0f) ? std::ceil(scaleMax) : 1.0f;
                float scale = rs * multiplier;
                if (scale < scaleMin) scale = scaleMin;
                if (scale > scaleMax) scale = scaleMax;

                auto ent = registry.create();
                auto& tc = registry.emplace<TransformComponent>(ent);
                tc.position = glm::vec3(x, y, z);
                tc.rotation = glm::vec3(0.0f, ry, 0.0f);
                tc.scale    = scale;

                // Attach visual mesh if this alias has a loaded model.
                if (it != modelMap.end()) {
                    auto& lm = it->second;
                    auto& smc       = registry.emplace<StaticModelComponent>(ent);
                    smc.model       = lm.model;
                    smc.boundingBox = new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId());
                    smc.textureIndex = 0;
                }

                // Assign deterministic network ID with collision resolution so
                // this entity and the editor_entities below all get distinct IDs,
                // matching the server's assignment order.
                uint32_t staticNetId = resolveStaticId(
                    generateStaticId(x, z), staticUsedIds, x, z);
                registry.emplace<NetworkIdComponent>(ent,
                    NetworkIdComponent{staticNetId, alias, false, 0});

                // Attach ColliderComponent AABB so EntityPicker can detect clicks.
                if (hasPrefabPhys) {
                    glm::vec3 scaledHalf = physHalfExtents * scale;
                    auto* box = new BoundingBox(nullptr, glm::vec3(1.0f));
                    box->setAABB(-scaledHalf, scaledHalf);
                    registry.emplace<ColliderComponent>(ent, ColliderComponent{box});
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Editor-placed entities — written by EditorSerializer::saveToJson.
    // Loaded after random entities so collision resolution against staticUsedIds
    // matches the baked chunk ordering (random first, then editor).
    // NPC prefabs (is_npc=true or ai_script present) are skipped here — they
    // arrive via SpawnPacket from the server and are created through
    // EntityFactory in Engine::onNetworkSpawn().  Creating them here would
    // produce a ghost entity with a different (static) network ID that never
    // receives server position updates, visible as a frozen duplicate.
    // -----------------------------------------------------------------------
    if (root.contains("editor_entities") && root["editor_entities"].is_array()) {
        for (auto& e : root["editor_entities"]) {
            std::string alias = e.value("alias", "");
            if (alias.empty()) continue;

            // Skip NPC prefabs — handled by the network layer.
            const auto& prefab = PrefabManager::get().getPrefab(alias);
            if (!prefab.is_null() &&
                (prefab.value("is_npc", false) || prefab.contains("ai_script")))
                continue;

            float x     = e.value("x",     0.0f);
            float z     = e.value("z",     0.0f);
            float ry    = e.value("ry",    0.0f);
            float scale = e.value("scale", 1.0f);
            float y     = e.value("y",     0.0f);
            if (primaryTerrain && y == 0.0f)
                y = primaryTerrain->getHeightOfTerrain(x, z);

            auto ent = registry.create();
            auto& tc = registry.emplace<TransformComponent>(ent);
            tc.position = glm::vec3(x, y, z);
            tc.rotation = glm::vec3(0.0f, ry, 0.0f);
            tc.scale    = scale;

            // Visual mesh — look up the model by alias.
            StringId aliasId(alias);
            auto mit = modelMap.find(aliasId);
            if (mit != modelMap.end()) {
                auto& lm = mit->second;
                auto& smc       = registry.emplace<StaticModelComponent>(ent);
                smc.model       = lm.model;
                smc.boundingBox = new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId());
                smc.textureIndex = 0;
            }

            // Deterministic network ID with collision resolution.
            uint32_t staticNetId = resolveStaticId(
                generateStaticId(x, z), staticUsedIds, x, z);
            registry.emplace<NetworkIdComponent>(ent,
                NetworkIdComponent{staticNetId, alias, false, 0});

            // ColliderComponent AABB from prefab physics for EntityPicker.
            if (!prefab.is_null() && prefab.contains("physics")) {
                const auto& phys = prefab["physics"];
                glm::vec3 physHalfExtents(0.5f);
                bool hasPrefabPhys = false;
                if (phys.contains("halfExtents") && phys["halfExtents"].is_array()
                        && phys["halfExtents"].size() >= 3) {
                    physHalfExtents = glm::vec3(
                        phys["halfExtents"][0].get<float>(),
                        phys["halfExtents"][1].get<float>(),
                        phys["halfExtents"][2].get<float>());
                    hasPrefabPhys = true;
                } else if (phys.contains("radius")) {
                    physHalfExtents = glm::vec3(phys.value("radius", 0.5f));
                    hasPrefabPhys = true;
                }
                if (hasPrefabPhys) {
                    glm::vec3 scaledHalf = physHalfExtents * scale;
                    auto* box = new BoundingBox(nullptr, glm::vec3(1.0f));
                    box->setAABB(-scaledHalf, scaledHalf);
                    registry.emplace<ColliderComponent>(ent, ColliderComponent{box});
                }
            }

            entityAliasByHandle[aliasId].push_back(ent);
        }
    }

    // -----------------------------------------------------------------------
    // Assimp entities
    // -----------------------------------------------------------------------
    if (root.contains("assimp") && root["assimp"].is_array()) {
        for (auto& a : root["assimp"]) {
            std::string path = a.value("path", "");
            bool  rndPos  = a.value("random", false);
            float yOff    = a.value("yOffset", 0.0f);
            float scMin   = a.value("scaleMin", 1.0f);
            float scMax   = a.value("scaleMax", 1.0f);
            float ax = a.value("x", 0.0f), ay = a.value("y", 0.0f), az = a.value("z", 0.0f);
            auto* mesh   = new AssimpMesh(path);
            auto  bbData = OBJLoader::loadBoundingBox(mesh, ClickBoxTypes::BOX, BoundTypes::AABB);
            auto* rawBb  = loader->loadToVAO(bbData);
            glm::vec3 pos = rndPos ? gRandomPosition(primaryTerrain, yOff) : glm::vec3(ax, ay, az);
            float sc = gRandomScale(scMin, scMax);
            auto assimpEnt = registry.create();
            registry.emplace<AssimpModelComponent>(assimpEnt, AssimpModelComponent{
                mesh, pos, gRandomRotation(), sc,
                new BoundingBox(rawBb, BoundingBoxIndex::genUniqueId())
            });
        }
    }

    // -----------------------------------------------------------------------
    // Player
    // -----------------------------------------------------------------------
    if (root.contains("player") && root["player"].is_object()) {
        auto& p = root["player"];
        std::string alias = p.value("alias", "");
        StringId aliasId(alias);
        auto it = modelMap.find(aliasId);
        if (it != modelMap.end()) {
            auto& lm = it->second;
            player = new Player(
                registry,
                lm.model,
                new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId()),
                glm::vec3(p.value("x", 0.0f), p.value("y", 0.0f), p.value("z", 0.0f)),
                glm::vec3(p.value("rx", 0.0f), p.value("ry", 0.0f), p.value("rz", 0.0f)),
                p.value("scale", 1.0f));
            InteractiveModel::setInteractiveBox(player);
            playerCamera = new PlayerCamera(player);
        } else {
            std::cerr << "[SceneLoaderJson] player references unknown alias '" << alias << "'\n";
        }
    }

    // -----------------------------------------------------------------------
    // GUI textures
    // -----------------------------------------------------------------------
    if (root.contains("gui") && root["gui"].is_array()) {
        for (auto& g : root["gui"]) {
            auto* t = loader->loadTexture(g.value("texture", ""));
            guis.push_back(new GuiTexture(t->getId(),
                glm::vec2(g.value("x", 0.0f), g.value("y", 0.0f)),
                glm::vec2(g.value("w", 0.0f), g.value("h", 0.0f))));
        }
    }

    // -----------------------------------------------------------------------
    // Water tiles
    // -----------------------------------------------------------------------
    if (root.contains("water") && root["water"].is_array()) {
        for (auto& w : root["water"])
            waterTiles.emplace_back(w.value("x", 0.0f), w.value("height", 0.0f), w.value("z", 0.0f));
    }

    // -----------------------------------------------------------------------
    // Text overlays
    // -----------------------------------------------------------------------
    if (root.contains("text") && root["text"].is_array() && !root["text"].empty()) {
        FontModel* sharedFontModel = loader->loadFontVAO();
        std::map<std::pair<std::string,int>, FontType*> fontCache;
        for (auto& td : root["text"]) {
            std::string fontName = td.value("font", "arial");
            int fontSize = td.value("size", 24);
            float tx = td.value("x", 0.0f), ty = td.value("y", 0.0f);
            float maxWidth = td.value("maxWidth", 1.0f);
            bool centered = td.value("centered", false);
            std::string message = td.value("message", "");
            float r = 1.0f, g = 1.0f, b = 1.0f;
            if (td.contains("color") && td["color"].is_array() && td["color"].size() >= 3) {
                r = td["color"][0].get<float>();
                g = td["color"][1].get<float>();
                b = td["color"][2].get<float>();
            }
            auto key = std::make_pair(fontName, fontSize);
            auto it = fontCache.find(key);
            if (it == fontCache.end()) {
                fontCache.emplace(key, new FontType(TextMeshData::loadFont(fontName, fontSize)));
                it = fontCache.find(key);
            }
            texts.push_back(new GUIText(message, 1.0f, sharedFontModel, it->second,
                glm::vec2(tx, ty), Color(r, g, b),
                maxWidth * static_cast<float>(DisplayManager::Width()), centered));
        }
    }

    // -----------------------------------------------------------------------
    // Animated characters
    // -----------------------------------------------------------------------
    auto normalizeClipName = [](const std::string& raw) -> std::string {
        static const char* known[] = {"Idle", "Walk", "Run", "Jump", nullptr};
        for (int i = 0; known[i]; ++i) {
            if (raw.size() == std::strlen(known[i])) {
                bool same = true;
                for (size_t k = 0; k < raw.size(); ++k)
                    same = same && (std::tolower((unsigned char)raw[k]) ==
                                    std::tolower((unsigned char)known[i][k]));
                if (same) return known[i];
            }
        }
        std::string lower(raw);
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.find("idle") != std::string::npos) return "Idle";
        if (lower.find("walk") != std::string::npos) return "Walk";
        if (lower.find("run")  != std::string::npos) return "Run";
        if (lower.find("jump") != std::string::npos) return "Jump";
        std::string out = raw;
        if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
        return out;
    };

    if (root.contains("animated_characters") && root["animated_characters"].is_array()) {
        for (auto& ac : root["animated_characters"]) {
            std::string relPath = ac.value("path", "");
            std::string absPath = FileSystem::Path("/src/Resources/Models/" + relPath);
            AnimatedModel* animModel = AnimationLoader::load(absPath);

            // If the primary path fails, try the optional fallback_path.
            // When the fallback is used, animation_map is NOT applied because the
            // fallback asset may have different clip names; normalizeClipName()
            // auto-detection is used instead.
            bool usingFallback = false;
            if (!animModel && ac.contains("fallback_path")) {
                const std::string fbRel = ac.value("fallback_path", "");
                const std::string fbAbs = FileSystem::Path("/src/Resources/Models/" + fbRel);
                std::cerr << "[SceneLoaderJson] Primary path '" << relPath
                          << "' failed to load; trying fallback '" << fbRel << "'\n";
                animModel = AnimationLoader::load(fbAbs);
                if (animModel) {
                    relPath      = fbRel;
                    usingFallback = true;
                }
            }

            if (!animModel) {
                std::cerr << "[SceneLoaderJson] Failed to load animated_character: "
                          << absPath << "\n";
                continue;
            }
            float ax = ac.value("x", 0.0f), az = ac.value("z", 0.0f);
            float yOff = 0.0f;
            float ay = ac.contains("y") ? parseJsonY(ac["y"], yOff) : 0.0f;
            float yVal = (ay == SceneLoader::kSnapY && primaryTerrain)
                ? primaryTerrain->getHeightOfTerrain(ax, az) + yOff : ay;
            float scale = ac.value("scale", 1.0f);
            float rx = 0, ry = 0, rz = 0, ox = 0, oy = 0, oz = 0;
            if (ac.contains("rot") && ac["rot"].is_object()) {
                rx = ac["rot"].value("rx", 0.0f);
                ry = ac["rot"].value("ry", 0.0f);
                rz = ac["rot"].value("rz", 0.0f);
            }
            if (ac.contains("offset") && ac["offset"].is_object()) {
                ox = ac["offset"].value("ox", 0.0f);
                oy = ac["offset"].value("oy", 0.0f);
                oz = ac["offset"].value("oz", 0.0f);
            }

            auto* controller = new AnimationController();
            bool idleRegistered = false;

            std::cout << "[SceneLoaderJson] Loaded animated_character '" << relPath
                      << "': " << animModel->clips.size() << " clip(s), "
                      << animModel->skeleton.getBoneCount() << " bone(s).\n";

            if (!usingFallback && ac.contains("animation_map") && ac["animation_map"].is_object()) {
                // Explicit animation_map: register ONLY the mapped states using
                // exact (case-sensitive) clip name lookup.  normalizeClipName() is
                // NOT called.  Clips absent from the map are silently ignored.
                const auto& amap = ac["animation_map"];
                for (auto& [stateName, clipNameVal] : amap.items()) {
                    const std::string clipName = clipNameVal.get<std::string>();
                    AnimationClip* foundClip = nullptr;
                    for (auto& clip : animModel->clips) {
                        if (clip.name == clipName) { foundClip = &clip; break; }
                    }
                    if (foundClip) {
                        controller->addState(stateName, foundClip);
                        if (stateName == "Idle") idleRegistered = true;
                        std::cout << "[SceneLoaderJson]   state '" << stateName
                                  << "' <- clip '" << clipName << "'\n";
                    } else {
                        std::cerr << "[SceneLoaderJson]   WARNING: animation_map"
                                     " entry '" << stateName << "' references clip '"
                                  << clipName << "' which was not found in '"
                                  << relPath << "'\n";
                    }
                }
            } else {
                // No animation_map: fall back to normalizeClipName() auto-detection
                // so all existing assets continue to work with zero config changes.
                for (auto& clip : animModel->clips) {
                    std::string stateName = normalizeClipName(clip.name);
                    controller->addState(stateName, &clip);
                    if (stateName == "Idle") idleRegistered = true;
                    std::cout << "[SceneLoaderJson]   state '" << stateName
                              << "' <- clip '" << clip.name << "'\n";
                }
            }

            if (idleRegistered) controller->setState("Idle");

            if (rx != 0.0f || ry != 0.0f || rz != 0.0f) {
                glm::mat4 userRot = glm::mat4(1.0f);
                userRot = glm::rotate(userRot, glm::radians(rx), glm::vec3(1, 0, 0));
                userRot = glm::rotate(userRot, glm::radians(ry), glm::vec3(0, 1, 0));
                userRot = glm::rotate(userRot, glm::radians(rz), glm::vec3(0, 0, 1));
                animModel->coordinateCorrection = userRot * animModel->coordinateCorrection;
            }

            // Create an ECS entity to carry this animated character.
            // isLocalPlayer is false here; Engine::loadScene() will mark all
            // entities loaded at startup as isLocalPlayer=true after this call.
            entt::entity animEnt = registry.create();
            auto& tc    = registry.emplace<TransformComponent>(animEnt);
            tc.position = glm::vec3(ax, yVal, az);
            tc.rotation = glm::vec3(rx, ry, rz);
            tc.scale    = scale;

            auto& amc       = registry.emplace<AnimatedModelComponent>(animEnt);
            amc.model       = animModel;
            amc.controller  = controller;
            amc.modelOffset = glm::vec3(ox, oy, oz);
            amc.scale       = scale;
            amc.ownsModel   = true;
            amc.isLocalPlayer = false;  // marked true by Engine after loading
            // coordinateCorrection was potentially adjusted above (userRot) — use
            // it as the default so AnimatedRenderer no longer needs to multiply it.
            amc.modelRotationMat = animModel->coordinateCorrection;
        }
    }

    // -----------------------------------------------------------------------
    // Physics bodies
    // -----------------------------------------------------------------------
    if (root.contains("physics_bodies") && root["physics_bodies"].is_array()) {
        for (auto& pb : root["physics_bodies"]) {
            std::string alias = pb.value("alias", "");
            StringId aliasId(alias);
            auto it = entityAliasByHandle.find(aliasId);
            if (it == entityAliasByHandle.end()) {
                std::cerr << "[SceneLoaderJson] physics_body references unknown alias '"
                          << alias << "'\n";
                continue;
            }
            std::string typeStr  = pb.value("type",  "static");
            std::string shapeStr = pb.value("shape", "box");
            BodyType bType = typeStr  == "dynamic"   ? BodyType::Dynamic
                           : typeStr  == "kinematic" ? BodyType::Kinematic
                           : BodyType::Static;
            ColliderShape bShape = shapeStr == "sphere"  ? ColliderShape::Sphere
                                 : shapeStr == "capsule" ? ColliderShape::Capsule
                                 : ColliderShape::Box;
            float mass        = pb.value("mass",        0.0f);
            float friction    = pb.value("friction",    0.5f);
            float restitution = pb.value("restitution", 0.3f);
            float radius      = pb.value("radius",      0.5f);
            float height      = pb.value("height",      1.8f);
            glm::vec3 halfExt = glm::vec3(0.5f);
            if (pb.contains("halfExtents") && pb["halfExtents"].is_array()
                    && pb["halfExtents"].size() >= 3) {
                halfExt.x = pb["halfExtents"][0].get<float>();
                halfExt.y = pb["halfExtents"][1].get<float>();
                halfExt.z = pb["halfExtents"][2].get<float>();
            }
            for (entt::entity handle : it->second) {
                PhysicsBodyCfg cfg;
                cfg.entityHandle = handle;  // direct ECS handle (no entityIndex)
                cfg.type        = bType;
                cfg.shape       = bShape;
                cfg.mass        = mass;
                cfg.halfExtents = halfExt;
                cfg.radius      = radius;
                cfg.height      = height;
                cfg.friction    = friction;
                cfg.restitution = restitution;
                physicsBodyCfgs.push_back(cfg);
            }
        }
    }

    if (root.contains("physics_ground") && root["physics_ground"].is_array()) {
        for (auto& pg : root["physics_ground"]) {
            PhysicsGroundCfg gcfg;
            gcfg.yHeight = pg.get<float>();
            physicsGroundCfgs.push_back(gcfg);
        }
    }

    // -----------------------------------------------------------------------
    // Final status log
    // -----------------------------------------------------------------------
    std::cout << "[SceneLoaderJson] Scene loaded: "
              << registry.view<StaticModelComponent>().size() << " static entities, "
              << registry.view<AssimpModelComponent>().size() << " assimp scenes, "
              << lights.size()           << " lights, "
              << allTerrains.size()      << " terrain tiles, "
              << guis.size()             << " GUI textures, "
              << texts.size()            << " text overlays, "
              << waterTiles.size()       << " water tiles, "
              << registry.view<AnimatedModelComponent>().size() << " animated characters.\n";
    return true;
}

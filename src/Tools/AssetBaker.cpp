// src/Tools/AssetBaker.cpp
//
// Offline CLI tool — Asset Conditioning Pipeline (GEA Step 5.1 / 5.2).
//
// Reads scene.json (entities + random arrays), loads ALL heightmaps into
// memory, pre-calculates exact Y coordinates, spatially partitions entities
// into 800×800 chunks, and writes binary .dat files that the runtime engine
// can load via a single memcpy.
//
// Also packs all resource files into a single data.pak archive (Step 5.2).
//
// Usage:  AssetBaker [--scene path/to/scene.json] [--out path/to/output/]
//         Defaults use RESOURCE_ROOT paths.

#include "../Streaming/ChunkData.h"
#include "../Terrain/HeightMap.h"
#include "../Util/FileSystem.h"
#include "../Toolbox/Maths.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cerrno>
#include <random>
#include <algorithm>
#include <filesystem>
#include <set>
#include <sys/stat.h>

// ============================================================================
// Headless terrain tile (same as ServerMain.cpp — duplicated here so the
// Baker is a standalone executable with no server dependencies).
// ============================================================================

struct BakerTerrain {
    std::vector<std::vector<float>> heights;
    float originX = 0.0f;
    float originZ = 0.0f;
    float size    = 800.0f;
    bool  valid   = false;

    void load(const std::string& heightmapPath, int gx, int gz) {
        Heightmap hm(heightmapPath);
        auto info = hm.getImageInfo();
        if (info.height <= 0 || info.width <= 0) {
            std::cerr << "[Baker] WARNING: Failed to load heightmap: "
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

struct BakerTerrainManager {
    std::vector<BakerTerrain> tiles;
    float terrainSize = 800.0f;

    void loadTile(const std::string& heightmapPath, int gx, int gz) {
        tiles.emplace_back();
        tiles.back().size = terrainSize;
        tiles.back().load(heightmapPath, gx, gz);
        if (!tiles.back().valid) {
            std::cerr << "[Baker] Skipping tile (" << gx << ", " << gz << ")\n";
            tiles.pop_back();
        }
    }

    bool isAnyValid() const {
        for (const auto& t : tiles) if (t.valid) return true;
        return false;
    }

    float getHeight(float worldX, float worldZ) const {
        for (const auto& tile : tiles) {
            if (!tile.valid) continue;
            if (worldX >= tile.originX && worldX < tile.originX + tile.size &&
                worldZ >= tile.originZ && worldZ < tile.originZ + tile.size) {
                return tile.getHeight(worldX, worldZ);
            }
        }
        for (const auto& tile : tiles) {
            if (tile.valid) return tile.getHeight(worldX, worldZ);
        }
        return 0.0f;
    }
};

// ============================================================================
// Spatial key helper
// ============================================================================

struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 16);
    }
};

// ============================================================================
// PAK archive generation (Step 5.2)
// ============================================================================

static void writePakArchive() {
    namespace fs = std::filesystem;

    std::cout << "[AssetBaker] === PAK Archive Generation (Step 5.2) ===\n";

    const std::string resourceDir = HOME_PATH + "/src/Resources";

    // File extensions to include in the archive
    const std::set<std::string> allowedExts = {
        ".png", ".obj", ".json", ".lua", ".cfg",
        ".ttf", ".fnt", ".glsl", ".vert", ".frag"
    };

    struct FileEntry {
        std::string relativePath; // relative to HOME_PATH, no leading slash
        std::string absolutePath;
        uint64_t    size   = 0;
        uint64_t    offset = 0;  // absolute offset in the pak file
    };

    std::vector<FileEntry> entries;
    std::set<std::string>  visitedDirs;

    // Scan the resources directory recursively
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(resourceDir, ec)) {
        // Print a message when entering each new directory
        std::string dirPath = entry.path().parent_path().string();
        if (visitedDirs.find(dirPath) == visitedDirs.end()) {
            std::cout << "[AssetBaker] Scanning directory: " << dirPath << "\n";
            visitedDirs.insert(dirPath);
        }

        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        if (allowedExts.find(ext) == allowedExts.end()) continue;

        FileEntry fe;
        fe.absolutePath = entry.path().string();
        fe.relativePath = fs::relative(entry.path(), HOME_PATH, ec).string();
        fe.size         = static_cast<uint64_t>(entry.file_size(ec));
        entries.push_back(fe);
    }
    if (ec) {
        std::cerr << "[AssetBaker] WARNING: Directory scan error: " << ec.message() << "\n";
    }

    std::cout << "[AssetBaker] Found " << entries.size() << " files to pack\n";

    // Calculate the size of the header + file table to determine data offsets:
    //   Magic(4) + Count(4) + for each file: PathLen(4) + Path(N) + Offset(8) + Size(8)
    uint64_t tableSize = 4 + 4; // magic + count
    for (const auto& fe : entries) {
        tableSize += 4 + static_cast<uint64_t>(fe.relativePath.size()) + 8 + 8;
    }

    // Assign absolute offsets for each file's data block
    uint64_t currentOffset = tableSize;
    for (auto& fe : entries) {
        fe.offset      = currentOffset;
        currentOffset += fe.size;
    }

    // Write data.pak to the project root
    const std::string pakPath = HOME_PATH + "/data.pak";
    std::ofstream out(pakPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "[AssetBaker] ERROR: Cannot write PAK file: " << pakPath << "\n";
        return;
    }

    // --- Header ---
    out.write("PAK1", 4);
    uint32_t count = static_cast<uint32_t>(entries.size());
    out.write(reinterpret_cast<const char*>(&count), 4);

    // --- File table ---
    for (const auto& fe : entries) {
        uint32_t pathLen = static_cast<uint32_t>(fe.relativePath.size());
        out.write(reinterpret_cast<const char*>(&pathLen), 4);
        out.write(fe.relativePath.c_str(), pathLen);
        out.write(reinterpret_cast<const char*>(&fe.offset), 8);
        out.write(reinterpret_cast<const char*>(&fe.size),   8);
    }

    // --- Raw data block ---
    char copyBuf[65536];
    for (const auto& fe : entries) {
        std::ifstream in(fe.absolutePath, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "[AssetBaker] WARNING: Cannot open for packing: "
                      << fe.absolutePath << "\n";
            // Write zero-padding to keep offsets consistent
            std::fill(std::begin(copyBuf), std::end(copyBuf), 0);
            for (uint64_t remaining = fe.size; remaining > 0; ) {
                std::streamsize chunk = static_cast<std::streamsize>(
                    std::min(remaining, static_cast<uint64_t>(sizeof(copyBuf))));
                out.write(copyBuf, chunk);
                remaining -= static_cast<uint64_t>(chunk);
            }
        } else {
            uint64_t remaining = fe.size;
            while (remaining > 0) {
                std::streamsize toRead = static_cast<std::streamsize>(
                    std::min(remaining, static_cast<uint64_t>(sizeof(copyBuf))));
                in.read(copyBuf, toRead);
                std::streamsize got = in.gcount();
                out.write(copyBuf, got);
                remaining -= static_cast<uint64_t>(got);
                if (got == 0) break;
            }
        }
        std::cout << "[AssetBaker] Packing: " << fe.relativePath
                  << " (" << fe.size << " bytes)\n";
    }

    std::cout << "[AssetBaker] PAK archive written: " << count << " files)\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Asset Baker — GEA Step 5.1 ===\n";

    // --- Parse command-line arguments ---
    std::string scenePath = FileSystem::Scene("scene.json");
    std::string outputDir = FileSystem::BakedChunk("");

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--scene" && i + 1 < argc) {
            scenePath = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            outputDir = argv[++i];
            if (!outputDir.empty() && outputDir.back() != '/')
                outputDir += '/';
        } else if (arg == "--help") {
            std::cout << "Usage: AssetBaker [--scene scene.json] [--out dir/]\n";
            return 0;
        }
    }

    // --- Load scene.json ---
    std::ifstream sceneFile(scenePath);
    if (!sceneFile.is_open()) {
        std::cerr << "[Baker] ERROR: Cannot open scene file: " << scenePath << "\n";
        return 1;
    }

    nlohmann::json root;
    try {
        sceneFile >> root;
    } catch (const std::exception& e) {
        std::cerr << "[Baker] ERROR: Failed to parse JSON: " << e.what() << "\n";
        return 1;
    }
    sceneFile.close();
    std::cout << "[Baker] Loaded scene: " << scenePath << "\n";

    // --- Load all heightmaps ---
    BakerTerrainManager terrainMgr;

    std::string hmBase = "heightMap";
    if (root.contains("terrain") && root["terrain"].contains("heightmap"))
        hmBase = root["terrain"]["heightmap"].get<std::string>();

    std::string hmPath = FileSystem::Texture(hmBase);

    if (root.contains("terrain_tiles") && root["terrain_tiles"].is_array()) {
        for (auto& tile : root["terrain_tiles"]) {
            int gx = tile.value("gridX", 0);
            int gz = tile.value("gridZ", -1);

            // Try per-tile heightmap first (heightMap_X_Z.png)
            std::string perTilePath = FileSystem::Texture(
                hmBase + "_" + std::to_string(gx) + "_" + std::to_string(gz));
            std::ifstream probe(perTilePath);
            if (probe.is_open()) {
                probe.close();
                terrainMgr.loadTile(perTilePath, gx, gz);
            } else {
                terrainMgr.loadTile(hmPath, gx, gz);
            }
        }
    }

    // Fallback
    if (!terrainMgr.isAnyValid()) {
        terrainMgr.loadTile(hmPath, 0, -1);
    }

    std::cout << "[Baker] Loaded " << terrainMgr.tiles.size() << " terrain tiles.\n";

    // --- Parse physics_bodies (only need alias set to know which entities to bake) ---
    std::unordered_map<std::string, bool> physBodies;
    if (root.contains("physics_bodies") && root["physics_bodies"].is_array()) {
        for (auto& pb : root["physics_bodies"]) {
            std::string alias = pb.value("alias", "");
            if (!alias.empty()) physBodies[alias] = true;
        }
    }

    // --- Collect all entities to bake ---
    std::vector<BakedEntity> allEntities;

    // --- Y-value parser (mirrors ServerMain) ---
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
                return terrainMgr.isAnyValid()
                    ? terrainMgr.getHeight(x, z) + offset : offset;
            }
            try { return std::stof(s); } catch (...) {}
        }
        return 0.0f;
    };

    // --- Fixed-position entities ---
    if (root.contains("entities") && root["entities"].is_array()) {
        for (auto& e : root["entities"]) {
            std::string alias = e.value("alias", "");
            uint32_t prefabId = BakedPrefab::fromAlias(alias);

            float x     = e.value("x",     0.0f);
            float z     = e.value("z",     0.0f);
            float ry    = e.value("ry",    0.0f);
            float scale = e.value("scale", 1.0f);
            float y     = parseY(e, x, z);

            BakedEntity be{};
            be.prefabId  = prefabId;
            be.x         = x;
            be.y         = y;
            be.z         = z;
            be.rotationY = ry;
            be.scale     = scale;
            allEntities.push_back(be);
        }
    }

    // --- Randomly-placed entities (mirrors SceneLoaderJson + ServerMain) ---
    if (root.contains("random") && root["random"].is_array()) {
        unsigned int seed = root.value("random_seed", 1u);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        auto randF = [&]() { return dist01(rng); };
        std::cout << "[Baker] Random seed: " << seed << "\n";

        for (auto& r : root["random"]) {
            std::string alias    = r.value("alias",    "");
            int         count    = r.value("count",    0);
            float       scaleMin = r.value("scaleMin", 0.75f);
            float       scaleMax = r.value("scaleMax", 1.5f);
            bool        useAtlas = r.value("atlas",    false);
            uint32_t    prefabId = BakedPrefab::fromAlias(alias);

            for (int i = 0; i < count; ++i) {
                float rx = randF();
                float rz = randF();
                float rr = randF();
                float rs = randF();
                if (useAtlas) randF(); // consume atlas draw to stay in sync

                float x  = std::floor(rx * 1500.f - 800.f);
                float z  = std::floor(rz * -800.f);
                float y  = terrainMgr.isAnyValid()
                         ? terrainMgr.getHeight(x, z) : 0.0f;
                float ry = (rr * 100.f - 50.f) * 180.0f;

                float multiplier = (scaleMax > 1.0f)
                                 ? std::ceil(scaleMax) : 1.0f;
                float scale = rs * multiplier;
                if (scale < scaleMin) scale = scaleMin;
                if (scale > scaleMax) scale = scaleMax;

                BakedEntity be{};
                be.prefabId  = prefabId;
                be.x         = x;
                be.y         = y;
                be.z         = z;
                be.rotationY = ry;
                be.scale     = scale;
                allEntities.push_back(be);
            }
        }
    }

    // --- Baker-generated random scattering (independent of scene.json) ---
    // When the scene.json "random" block has been removed, the baker still
    // scatters entities using its own built-in configuration.  This makes the
    // baker the single source of truth for world population.
    {
        unsigned int bakerSeed = root.value("random_seed", 42u);
        std::mt19937 bakerRng(bakerSeed + 1000); // offset seed to avoid collisions
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        auto randF = [&]() { return dist01(bakerRng); };

        struct BakerScatter {
            uint32_t prefabId;
            int      count;
            float    scaleMin;
            float    scaleMax;
        };

        std::vector<BakerScatter> scatters = {
            { BakedPrefab::Tree,    500, 0.25f, 1.5f },
            { BakedPrefab::Boulder, 200, 0.75f, 2.0f },
        };

        for (const auto& sc : scatters) {
            for (int i = 0; i < sc.count; ++i) {
                float rx = randF();
                float rz = randF();
                float rr = randF();
                float rs = randF();

                float x  = std::floor(rx * 1500.f - 800.f);
                float z  = std::floor(rz * -800.f);
                float y  = terrainMgr.isAnyValid()
                         ? terrainMgr.getHeight(x, z) : 0.0f;
                // Random Y rotation in degrees (mirrors SceneLoaderJson RNG).
                float ry = (rr * 100.f - 50.f) * 180.0f;

                // Scale with clamping (mirrors SceneLoaderJson RNG algorithm
                // for bit-identical results with the same seed).
                float multiplier = (sc.scaleMax > 1.0f)
                                 ? std::ceil(sc.scaleMax) : 1.0f;
                float scale = rs * multiplier;
                if (scale < sc.scaleMin) scale = sc.scaleMin;
                if (scale > sc.scaleMax) scale = sc.scaleMax;

                BakedEntity be{};
                be.prefabId  = sc.prefabId;
                be.x         = x;
                be.y         = y;
                be.z         = z;
                be.rotationY = ry;
                be.scale     = scale;
                allEntities.push_back(be);
            }
        }
        std::cout << "[Baker] Baker-generated scatter: trees + boulders.\n";
    }

    std::cout << "[Baker] Collected " << allEntities.size()
              << " entities total.\n";

    // --- Spatial partitioning: group by chunk grid ---
    std::unordered_map<std::pair<int,int>, std::vector<BakedEntity>, PairHash>
        chunkBuckets;

    for (const auto& be : allEntities) {
        int cx = static_cast<int>(
            std::floor(be.x / terrainMgr.terrainSize));
        int cz = static_cast<int>(
            std::floor(be.z / terrainMgr.terrainSize));
        chunkBuckets[{cx, cz}].push_back(be);
    }

    // --- Create output directory ---
    int mkdirResult;
#ifdef _WIN32
    mkdirResult = _mkdir(outputDir.c_str());
#else
    mkdirResult = mkdir(outputDir.c_str(), 0755);
#endif
    if (mkdirResult != 0 && errno != EEXIST) {
        std::cerr << "[Baker] WARNING: Could not create output directory: "
                  << outputDir << " (errno=" << errno << ")\n";
    }

    // --- Write binary .dat files ---
    int filesWritten = 0;
    for (auto& [key, entities] : chunkBuckets) {
        std::string filename = "chunk_" + std::to_string(key.first)
                             + "_" + std::to_string(key.second) + ".dat";
        std::string fullPath = outputDir + filename;

        if (writeBakedChunk(fullPath, key.first, key.second, entities)) {
            std::cout << "[Baker] Wrote " << filename
                      << " (" << entities.size() << " entities)\n";
            ++filesWritten;
        } else {
            std::cerr << "[Baker] ERROR: Failed to write " << fullPath << "\n";
        }
    }

    std::cout << "=== Asset Baker complete: Processed "
              << allEntities.size() << " entities. Generated "
              << filesWritten << " chunk files. ===\n";

    // --- Step 5.2: PAK archive generation ---
    writePakArchive();

    return 0;
}

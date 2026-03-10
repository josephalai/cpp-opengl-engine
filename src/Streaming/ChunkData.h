// src/Streaming/ChunkData.h
//
// Binary data structures for pre-baked chunk entity data.
// Written by the offline AssetBaker tool and loaded at runtime by
// ChunkManager / HeadlessTerrainManager for instantaneous entity spawning.
//
// These structs are written/read via a single memcpy — no string parsing,
// no JSON overhead.  All positions are pre-calculated (including Y heights).

#ifndef ENGINE_CHUNKDATA_H
#define ENGINE_CHUNKDATA_H

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

// ============================================================================
// On-disk binary format
// ============================================================================

/// A single pre-baked entity to be spawned in a chunk.
struct BakedEntity {
    uint32_t prefabId;      ///< e.g. 101 for "tree", 102 for "npc_wanderer"
    float    x, y, z;       ///< Pre-calculated absolute world position
    float    rotationY;     ///< Y-axis rotation in degrees
    float    scale;         ///< Uniform scale factor
};

/// Header written at the start of each chunk .dat file.
struct BakedChunkHeader {
    uint32_t magic;         ///< File magic: 'BCHK' = 0x4B434842
    uint32_t version;       ///< Format version (currently 1)
    uint32_t entityCount;   ///< Number of BakedEntity records following
    int32_t  gridX;         ///< Chunk grid X coordinate
    int32_t  gridZ;         ///< Chunk grid Z coordinate
};

static constexpr uint32_t kBakedChunkMagic   = 0x4B434842; // 'BCHK'
static constexpr uint32_t kBakedChunkVersion = 1;

// ============================================================================
// Alias-to-prefabId mapping (shared between Baker and Runtime)
// ============================================================================

namespace BakedPrefab {
    /// Well-known prefab IDs used by the Asset Baker.  The runtime maps these
    /// back to physics body configurations or rendering prefabs.
    enum Id : uint32_t {
        Unknown    = 0,
        Tree       = 101,
        FluffyTree = 102,
        Grass      = 103,
        Fern       = 104,
        Lamp       = 105,
        Stall      = 106,
        Crate      = 107,
    };

    /// Convert a scene.json alias string to a prefab ID.
    inline uint32_t fromAlias(const std::string& alias) {
        if (alias == "tree")       return Tree;
        if (alias == "fluffytree") return FluffyTree;
        if (alias == "grass")      return Grass;
        if (alias == "fern")       return Fern;
        if (alias == "lamp")       return Lamp;
        if (alias == "stall")      return Stall;
        if (alias == "crate")      return Crate;
        return Unknown;
    }

    /// Convert a prefab ID back to the scene.json alias string.
    inline std::string toAlias(uint32_t id) {
        switch (id) {
            case Tree:       return "tree";
            case FluffyTree: return "fluffytree";
            case Grass:      return "grass";
            case Fern:       return "fern";
            case Lamp:       return "lamp";
            case Stall:      return "stall";
            case Crate:      return "crate";
            default:         return "";
        }
    }
}

// ============================================================================
// I/O helpers
// ============================================================================

/// Write a chunk's baked data to a binary file.
inline bool writeBakedChunk(const std::string& path,
                            int32_t gridX, int32_t gridZ,
                            const std::vector<BakedEntity>& entities) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[BakedChunk] ERROR: Cannot open for writing: " << path << "\n";
        return false;
    }

    BakedChunkHeader header{};
    header.magic       = kBakedChunkMagic;
    header.version     = kBakedChunkVersion;
    header.entityCount = static_cast<uint32_t>(entities.size());
    header.gridX       = gridX;
    header.gridZ       = gridZ;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!file.good()) {
        std::cerr << "[BakedChunk] ERROR: Failed to write header to: " << path << "\n";
        return false;
    }
    if (!entities.empty()) {
        file.write(reinterpret_cast<const char*>(entities.data()),
                   static_cast<std::streamsize>(entities.size() * sizeof(BakedEntity)));
    }
    return file.good();
}

/// Maximum entities per chunk — prevents OOM from malformed files.
static constexpr uint32_t kMaxEntitiesPerChunk = 1000000;

/// Read a chunk's baked data from a binary file.
/// Returns true on success, false if the file doesn't exist or is malformed.
inline bool readBakedChunk(const std::string& path,
                           BakedChunkHeader& outHeader,
                           std::vector<BakedEntity>& outEntities) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.read(reinterpret_cast<char*>(&outHeader), sizeof(outHeader));
    if (!file.good()) return false;

    // Validate magic and version.
    if (outHeader.magic != kBakedChunkMagic) {
        std::cerr << "[BakedChunk] ERROR: Bad magic in " << path << "\n";
        return false;
    }
    if (outHeader.version != kBakedChunkVersion) {
        std::cerr << "[BakedChunk] ERROR: Unsupported version "
                  << outHeader.version << " in " << path << "\n";
        return false;
    }

    // Validate entity count to prevent OOM from malformed files.
    if (outHeader.entityCount > kMaxEntitiesPerChunk) {
        std::cerr << "[BakedChunk] ERROR: entityCount " << outHeader.entityCount
                  << " exceeds maximum " << kMaxEntitiesPerChunk
                  << " in " << path << "\n";
        return false;
    }

    outEntities.resize(outHeader.entityCount);
    if (outHeader.entityCount > 0) {
        file.read(reinterpret_cast<char*>(outEntities.data()),
                  static_cast<std::streamsize>(outHeader.entityCount * sizeof(BakedEntity)));
    }
    return file.good();
}

#endif // ENGINE_CHUNKDATA_H

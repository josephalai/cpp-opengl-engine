// src/Engine/TileGrid.h
// Tile-based placement helper for the World Editor.
//
// The world is divided into a uniform grid of square cells whose side length
// is `tileSize` world units.  The tile grid is used for two purposes:
//
//   1. Snapping — the ghost preview jumps to the nearest tile centre so
//      objects align to the grid as the mouse moves.
//
//   2. Overlap prevention — before spawning an entity the caller should invoke
//      isPlacementValid() which checks the XZ AABB of the incoming entity
//      against every existing EditorPlacedComponent entity in the registry.
//      If any AABB pair overlaps, placement is rejected.
//
// Design notes
//  • No persistent occupancy state is stored here; validity is checked live
//    against the ECS registry so it stays consistent when entities are deleted
//    or moved via the transform editor.
//  • Only the XZ plane is tested (Y is irrelevant for top-down tile placement).

#ifndef ENGINE_TILEGRID_H
#define ENGINE_TILEGRID_H

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <cmath>
#include <utility>
#include <string>
#include <unordered_set>

/// Identifies one tile cell by its integer grid coordinates.
struct TileCoord {
    int x = 0;
    int z = 0;

    bool operator==(const TileCoord& o) const { return x == o.x && z == o.z; }
};

struct TileCoordHash {
    std::size_t operator()(const TileCoord& t) const noexcept {
        // Combine two 32-bit ints into a 64-bit hash.
        return std::hash<long long>{}(
            (static_cast<long long>(t.x) << 32) |
            static_cast<unsigned int>(t.z));
    }
};

using TileSet = std::unordered_set<TileCoord, TileCoordHash>;

class TileGrid {
public:
    // -----------------------------------------------------------------------
    // Coordinate conversion
    // -----------------------------------------------------------------------

    /// Convert a world-space position to its integer tile coordinates.
    static TileCoord worldToTile(float x, float z, float tileSize) {
        // Round to nearest tile: floor(coord / tileSize + 0.5)
        return {
            static_cast<int>(std::floor(x / tileSize + 0.5f)),
            static_cast<int>(std::floor(z / tileSize + 0.5f))
        };
    }

    /// Return the world-space centre of a tile.
    static glm::vec2 tileCenter(int tileX, int tileZ, float tileSize) {
        return { static_cast<float>(tileX) * tileSize,
                 static_cast<float>(tileZ) * tileSize };
    }

    /// Snap a world position to the nearest tile centre.
    /// The Y component is preserved unchanged.
    static glm::vec3 snapToTile(const glm::vec3& worldPos, float tileSize) {
        TileCoord tc = worldToTile(worldPos.x, worldPos.z, tileSize);
        glm::vec2 centre = tileCenter(tc.x, tc.z, tileSize);
        return { centre.x, worldPos.y, centre.y };
    }

    /// Snap a world position to the nearest tile boundary that keeps the entity
    /// AABB aligned to the grid.
    ///
    /// • Entities whose footprint spans an *odd* number of tiles along an axis
    ///   have their centre snapped to the nearest tile centre (same as snapToTile).
    /// • Entities whose footprint spans an *even* number of tiles along an axis
    ///   have their centre snapped to the nearest tile junction (the boundary
    ///   between two adjacent tiles) so that the AABB edges fall on tile lines.
    ///
    /// @param halfX  XZ half-extent along X (already multiplied by scale).
    /// @param halfZ  XZ half-extent along Z (already multiplied by scale).
    static glm::vec3 snapToGrid(const glm::vec3& worldPos,
                                float tileSize,
                                float halfX, float halfZ) {
        // Helper: snap a single axis value.
        auto snapAxis = [](float v, float ts, float half) -> float {
            // Number of full tile widths the footprint spans.
            int nTiles = static_cast<int>(std::ceil(2.0f * half / ts));
            if (nTiles % 2 == 1) {
                // Odd — centre must land on a tile centre.
                return std::floor(v / ts + 0.5f) * ts;
            } else {
                // Even — centre must land on a tile junction (boundary).
                // Tile junctions are at (k + 0.5)*ts; nearest junction:
                //   floor((v - ts/2) / ts + 0.5) * ts + ts/2
                return std::floor((v - ts * 0.5f) / ts + 0.5f) * ts + ts * 0.5f;
            }
        };
        return {
            snapAxis(worldPos.x, tileSize, halfX),
            worldPos.y,
            snapAxis(worldPos.z, tileSize, halfZ)
        };
    }

    // -----------------------------------------------------------------------
    // Footprint helpers
    // -----------------------------------------------------------------------

    /// Return the set of tile cells that overlap with the XZ AABB:
    ///   [cx - halfX, cx + halfX] × [cz - halfZ, cz + halfZ]
    ///
    /// A small epsilon is subtracted from the corners so that an AABB edge
    /// that falls exactly on a tile boundary does not bleed into the tile on
    /// the far side.  This keeps the tile count consistent with the intuitive
    /// footprint width and matches the epsilon used in isPlacementValid().
    static TileSet footprintTiles(float cx, float cz,
                                  float halfX, float halfZ,
                                  float tileSize) {
        TileSet tiles;
        // Guard against non-finite half-extents (e.g. from a broken AABB).
        if (!std::isfinite(halfX) || !std::isfinite(halfZ) ||
            halfX <= 0.0f || halfZ <= 0.0f) {
            return tiles;
        }
        constexpr float kEps = 1e-4f;
        // Shrink corners slightly so an AABB edge exactly on a tile boundary
        // is not mapped to the tile on the other side of that boundary.
        TileCoord minTile = worldToTile(cx - halfX + kEps, cz - halfZ + kEps, tileSize);
        TileCoord maxTile = worldToTile(cx + halfX - kEps, cz + halfZ - kEps, tileSize);
        for (int tx = minTile.x; tx <= maxTile.x; ++tx) {
            for (int tz = minTile.z; tz <= maxTile.z; ++tz) {
                tiles.insert({tx, tz});
            }
        }
        return tiles;
    }

    // -----------------------------------------------------------------------
    // Occupancy (live query against ECS registry)
    // -----------------------------------------------------------------------

    /// Build a set of all tile cells occupied by EditorPlacedComponent entities
    /// that have a physics footprint defined in their prefab JSON.
    ///
    /// @param excludeEntity  If not entt::null, skip this entity (used when
    ///                        moving an already-placed entity).
    static TileSet buildOccupancy(const entt::registry& registry,
                                  float tileSize,
                                  entt::entity excludeEntity = entt::null);

    // -----------------------------------------------------------------------
    // Placement validation
    // -----------------------------------------------------------------------

    /// Returns true when the XZ AABB of a proposed entity placement does NOT
    /// overlap with any existing EditorPlacedComponent entity in the registry.
    ///
    /// @param ghostPos   World-space position of the proposed entity.
    /// @param halfX      XZ half-extent along X (from prefab, already × scale).
    /// @param halfZ      XZ half-extent along Z (from prefab, already × scale).
    static bool isPlacementValid(const entt::registry& registry,
                                 const glm::vec3& ghostPos,
                                 float halfX, float halfZ,
                                 entt::entity excludeEntity = entt::null);
};

#endif // ENGINE_TILEGRID_H

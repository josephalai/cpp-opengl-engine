// src/Streaming/SpatialGrid.h
//
// Phase 4 Step 4.1.1 — Interest-management grid that divides the world into
// fixed-size network cells (default 50 m × 50 m).  Each cell holds separate
// lists of static and dynamic entities so that Area-of-Interest (AoI) queries
// can quickly find all entities near a given position.
//
// The grid is sparse (backed by an unordered_map) so it works for worlds of
// any size.  For a single 800 m chunk the grid covers a 16×16 area.

#ifndef ENGINE_SPATIALGRID_H
#define ENGINE_SPATIALGRID_H

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

// -------------------------------------------------------------------------
// SpatialCell — a single grid tile holding entity references.
// -------------------------------------------------------------------------
struct SpatialCell {
    std::vector<entt::entity> staticEntities;   ///< Trees, rocks, buildings.
    std::vector<entt::entity> dynamicEntities;  ///< Players, NPCs.
};

// -------------------------------------------------------------------------
// SpatialGrid — sparse 2D grid of SpatialCells.
// -------------------------------------------------------------------------
class SpatialGrid {
public:
    /// @param cellSize  World-space side length of each cell (default 50 m).
    explicit SpatialGrid(float cellSize = 50.0f) : cellSize_(cellSize) {}

    /// Convert a world-space position to its grid-cell coordinates.
    void worldToCell(float x, float z, int& cellX, int& cellZ) const {
        cellX = static_cast<int>(std::floor(x / cellSize_));
        cellZ = static_cast<int>(std::floor(z / cellSize_));
    }

    /// Add an entity to the grid.
    /// @param isStatic  true for trees/rocks; false for players/NPCs.
    void addEntity(entt::entity e, int cellX, int cellZ, bool isStatic) {
        auto& cell = getOrCreateCell(cellX, cellZ);
        if (isStatic)
            cell.staticEntities.push_back(e);
        else
            cell.dynamicEntities.push_back(e);
    }

    /// Remove an entity from a specific cell.
    void removeEntity(entt::entity e, int cellX, int cellZ) {
        auto key = makeKey(cellX, cellZ);
        auto it = cells_.find(key);
        if (it == cells_.end()) return;

        auto& cell = it->second;
        eraseEntity(cell.staticEntities, e);
        eraseEntity(cell.dynamicEntities, e);
    }

    /// Move an entity from one cell to another.
    void migrateEntity(entt::entity e, int oldCX, int oldCZ,
                       int newCX, int newCZ, bool isStatic) {
        removeEntity(e, oldCX, oldCZ);
        addEntity(e, newCX, newCZ, isStatic);
    }

    /// Query all entities in a cell and its 8 neighbours (3×3 neighbourhood).
    /// This is the Area-of-Interest (AoI) query used by the network layer.
    std::vector<entt::entity> queryNeighbourhood(int cellX, int cellZ) const {
        std::vector<entt::entity> result;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                auto key = makeKey(cellX + dx, cellZ + dz);
                auto it = cells_.find(key);
                if (it == cells_.end()) continue;
                const auto& cell = it->second;
                result.insert(result.end(),
                              cell.staticEntities.begin(),
                              cell.staticEntities.end());
                result.insert(result.end(),
                              cell.dynamicEntities.begin(),
                              cell.dynamicEntities.end());
            }
        }
        return result;
    }

    /// Get a read-only pointer to a specific cell (nullptr if empty/missing).
    const SpatialCell* getCell(int cellX, int cellZ) const {
        auto key = makeKey(cellX, cellZ);
        auto it = cells_.find(key);
        return (it != cells_.end()) ? &it->second : nullptr;
    }

    /// Return all non-empty cells (for debugging / full iteration).
    const auto& allCells() const { return cells_; }

    float cellSize() const { return cellSize_; }

    void clear() { cells_.clear(); }

private:
    float cellSize_;

    struct PairHash {
        std::size_t operator()(const std::pair<int,int>& p) const {
            auto h1 = std::hash<int>()(p.first);
            auto h2 = std::hash<int>()(p.second);
            return h1 ^ (h2 * 2654435761u);
        }
    };
    std::unordered_map<std::pair<int,int>, SpatialCell, PairHash> cells_;

    static std::pair<int,int> makeKey(int x, int z) { return {x, z}; }

    SpatialCell& getOrCreateCell(int x, int z) {
        return cells_[makeKey(x, z)];
    }

    static void eraseEntity(std::vector<entt::entity>& vec, entt::entity e) {
        auto it = std::find(vec.begin(), vec.end(), e);
        if (it != vec.end()) {
            // Swap-and-pop for O(1) removal (order doesn't matter).
            std::swap(*it, vec.back());
            vec.pop_back();
        }
    }
};

#endif // ENGINE_SPATIALGRID_H

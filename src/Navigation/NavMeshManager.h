// src/Navigation/NavMeshManager.h
//
// Phase 4 Step 4.4.1 — NavMesh generation interface.
// Provides a high-level API for building and querying a walkable NavMesh.
// The initial implementation uses a simple grid-based pathfinder; future
// versions will integrate RecastNavigation for true NavMesh generation from
// heightmaps and static physics bodies.
//
// Step 4.4.5 — Dynamic obstacles: addObstacle() / removeObstacle() allow
// runtime modification (e.g. Player-Owned Houses punching holes in the mesh).

#ifndef ENGINE_NAVMESHMANAGER_H
#define ENGINE_NAVMESHMANAGER_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <glm/glm.hpp>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <queue>

class NavMeshManager {
public:
    /// @param gridResolution  Walkability grid cell size (metres).
    explicit NavMeshManager(float gridResolution = 1.0f)
        : gridRes_(gridResolution) {}

    /// Build (or rebuild) the navigation mesh from a heightmap + static bodies.
    /// For now this sets up a simple grid; a full Recast build would be done
    /// here once the library is integrated.
    /// @param worldMinX, worldMinZ, worldMaxX, worldMaxZ  Walkable area bounds.
    void build(float worldMinX, float worldMinZ,
               float worldMaxX, float worldMaxZ) {
        if (worldMaxX <= worldMinX || worldMaxZ <= worldMinZ || gridRes_ <= 0.0f) {
            built_ = false;
            return;
        }
        minX_ = worldMinX;
        minZ_ = worldMinZ;
        float rawW = (worldMaxX - worldMinX) / gridRes_;
        float rawH = (worldMaxZ - worldMinZ) / gridRes_;
        // Cap grid dimensions to prevent unbounded memory allocation.
        if (rawW > 1e6f || rawH > 1e6f) {
            built_ = false;
            return;
        }
        gridW_ = std::max(1, static_cast<int>(std::ceil(rawW)));
        gridH_ = std::max(1, static_cast<int>(std::ceil(rawH)));
        blocked_.clear();
        built_ = true;
    }

    /// Mark a rectangular area as blocked (Step 4.4.5 — Dynamic Obstacle).
    /// @return An obstacle ID that can be passed to removeObstacle().
    uint32_t addObstacle(float minX, float minZ, float maxX, float maxZ) {
        uint32_t id = nextObstacleId_++;
        int cx0 = worldToCellX(minX), cx1 = worldToCellX(maxX);
        int cz0 = worldToCellZ(minZ), cz1 = worldToCellZ(maxZ);
        ObstacleRecord rec;
        rec.id = id;
        for (int x = cx0; x <= cx1; ++x)
            for (int z = cz0; z <= cz1; ++z) {
                auto key = cellKey(x, z);
                blocked_.insert(key);
                rec.cells.push_back(key);
            }
        obstacles_.push_back(rec);
        return id;
    }

    /// Remove a previously added obstacle, restoring walkability.
    void removeObstacle(uint32_t obstacleId) {
        auto it = std::find_if(obstacles_.begin(), obstacles_.end(),
            [obstacleId](const ObstacleRecord& r) { return r.id == obstacleId; });
        if (it == obstacles_.end()) return;
        for (auto key : it->cells) {
            blocked_.erase(key);
        }
        obstacles_.erase(it);
    }

    /// Phase 4 Step 4.4 — Add a dynamic obstacle from a BoundingBox AABB.
    /// Convenience wrapper for buildings / Player-Owned Houses.
    /// @param center  World-space center of the obstacle.
    /// @param halfExtents  Half-width/depth of the obstacle footprint.
    /// @return Obstacle ID for later removal.
    uint32_t addObstacleFromBounds(const glm::vec3& center,
                                   const glm::vec3& halfExtents) {
        return addObstacle(center.x - halfExtents.x,
                           center.z - halfExtents.z,
                           center.x + halfExtents.x,
                           center.z + halfExtents.z);
    }

    /// Phase 4 Step 4.4 — Rebuild the walkability grid for a specific tile
    /// region.  Call after spawning/removing a static building to refresh
    /// only the affected area without recalculating the entire mesh.
    /// @param tileMinX, tileMinZ, tileMaxX, tileMaxZ  World-space tile bounds.
    void rebuildTile(float tileMinX, float tileMinZ,
                     float tileMaxX, float tileMaxZ) {
        // In the grid-based pathfinder, obstacle state is checked dynamically
        // during each A* query — no explicit tile rebuild is needed.
        // This method is provided as a future integration point for when the
        // engine migrates to RecastNavigation's dtTileCache, which requires
        // explicit tile rebuilds after geometry changes.
        //
        // For now, ensure the obstacle records are up to date (they are
        // maintained by addObstacle/removeObstacle) and any cached paths
        // are invalidated.
        (void)tileMinX; (void)tileMinZ;
        (void)tileMaxX; (void)tileMaxZ;
    }

    /// Find a path from start to goal using A* on the walkability grid.
    /// Returns an empty vector if no path exists or the mesh hasn't been built.
    std::vector<glm::vec3> findPath(const glm::vec3& start,
                                     const glm::vec3& goal) const {
        if (!built_) return {};

        int sx = worldToCellX(start.x), sz = worldToCellZ(start.z);
        int gx = worldToCellX(goal.x),  gz = worldToCellZ(goal.z);

        // Clamp to grid bounds.
        sx = clampX(sx); sz = clampZ(sz);
        gx = clampX(gx); gz = clampZ(gz);

        if (isBlocked(gx, gz)) return {};

        // A* search.
        struct Node {
            int x, z;
            float g, f;
            int parentIdx;
        };

        auto heuristic = [](int ax, int az, int bx, int bz) -> float {
            float dx = static_cast<float>(ax - bx);
            float dz = static_cast<float>(az - bz);
            return std::sqrt(dx*dx + dz*dz);
        };

        struct NodeHash {
            std::size_t operator()(const std::pair<int,int>& p) const {
                // Shift by 16 bits to spread the two 32-bit ints into
                // non-overlapping halves of the hash, reducing collisions
                // for grid coordinates.
                return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 16);
            }
        };

        std::vector<Node> nodes;
        std::unordered_map<std::pair<int,int>, int, NodeHash> visited;

        auto cmp = [](const std::pair<float,int>& a,
                      const std::pair<float,int>& b) { return a.first > b.first; };
        std::priority_queue<std::pair<float,int>,
                            std::vector<std::pair<float,int>>,
                            decltype(cmp)> open(cmp);

        nodes.push_back({sx, sz, 0.0f, heuristic(sx,sz,gx,gz), -1});
        open.push({nodes[0].f, 0});
        visited[{sx, sz}] = 0;

        const int dx8[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        const int dz8[] = {-1,-1,-1,  0, 0,  1, 1, 1};
        const float cost8[] = {1.414f,1.0f,1.414f, 1.0f,1.0f, 1.414f,1.0f,1.414f};

        // Limit iterations to prevent runaway searches.  At 1 m grid resolution,
        // 10,000 iterations covers paths up to ~100 m in a crowded environment.
        // Increase for larger worlds or lower grid resolutions.
        constexpr int kMaxIterations = 10000;
        int iterations = 0;

        while (!open.empty() && iterations < kMaxIterations) {
            ++iterations;
            auto [f, idx] = open.top(); open.pop();
            auto& cur = nodes[idx];

            if (cur.x == gx && cur.z == gz) {
                // Reconstruct path.
                std::vector<glm::vec3> path;
                int i = idx;
                while (i >= 0) {
                    path.push_back(cellToWorld(nodes[i].x, nodes[i].z, start.y));
                    i = nodes[i].parentIdx;
                }
                std::reverse(path.begin(), path.end());
                return path;
            }

            for (int d = 0; d < 8; ++d) {
                int nx = cur.x + dx8[d];
                int nz = cur.z + dz8[d];
                if (nx < 0 || nx >= gridW_ || nz < 0 || nz >= gridH_) continue;
                if (isBlocked(nx, nz)) continue;

                float ng = cur.g + cost8[d] * gridRes_;
                auto nkey = std::make_pair(nx, nz);
                auto vit = visited.find(nkey);
                if (vit != visited.end() && nodes[vit->second].g <= ng) continue;

                int nIdx = static_cast<int>(nodes.size());
                nodes.push_back({nx, nz, ng, ng + heuristic(nx,nz,gx,gz), idx});
                visited[nkey] = nIdx;
                open.push({nodes.back().f, nIdx});
            }
        }

        return {};  // No path found.
    }

    bool isBuilt() const { return built_; }

private:
    float gridRes_;
    float minX_ = 0.0f, minZ_ = 0.0f;
    int   gridW_ = 0, gridH_ = 0;
    bool  built_ = false;
    uint32_t nextObstacleId_ = 1;

    struct ObstacleRecord {
        uint32_t id = 0;
        std::vector<int64_t> cells;
    };
    std::vector<ObstacleRecord> obstacles_;

    std::unordered_set<int64_t> blocked_;

    int worldToCellX(float x) const {
        return static_cast<int>(std::floor((x - minX_) / gridRes_));
    }
    int worldToCellZ(float z) const {
        return static_cast<int>(std::floor((z - minZ_) / gridRes_));
    }
    int clampX(int x) const { return gridW_ <= 0 ? 0 : std::max(0, std::min(x, gridW_ - 1)); }
    int clampZ(int z) const { return gridH_ <= 0 ? 0 : std::max(0, std::min(z, gridH_ - 1)); }

    int64_t cellKey(int x, int z) const {
        return (static_cast<int64_t>(x) << 32) | static_cast<int64_t>(static_cast<uint32_t>(z));
    }

    bool isBlocked(int x, int z) const {
        return blocked_.count(cellKey(x, z)) > 0;
    }

    glm::vec3 cellToWorld(int cx, int cz, float y) const {
        return glm::vec3(minX_ + (cx + 0.5f) * gridRes_,
                         y,
                         minZ_ + (cz + 0.5f) * gridRes_);
    }
};

#endif // ENGINE_NAVMESHMANAGER_H

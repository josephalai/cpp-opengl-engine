// src/Engine/TileGrid.cpp

#include "TileGrid.h"

#include "../ECS/Components/EditorPlacedComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../Config/PrefabManager.h"

#include <nlohmann/json.hpp>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal helper — get XZ half-extents (already multiplied by entity scale)
// from a prefab's physics definition.  Returns {0,0} if none is found.
// ---------------------------------------------------------------------------
static glm::vec2 prefabFootprint(const std::string& alias, float scale) {
    // Prefer mesh AABB (full visual bounds including canopy/leaves).
    glm::vec2 meshHE = PrefabManager::get().getMeshHalfExtentsXZ(alias, scale);
    if (meshHE.x > 0.0f && meshHE.y > 0.0f) {
        return meshHE;
    }

    const auto& j = PrefabManager::get().getPrefab(alias);
    if (j.is_null()) return {0.0f, 0.0f};

    if (j.contains("physics") && j["physics"].contains("halfExtents")) {
        const auto& he = j["physics"]["halfExtents"];
        if (he.is_array() && he.size() >= 3) {
            float hx = he[0].get<float>() * scale;
            float hz = he[2].get<float>() * scale;
            return {hx, hz};
        }
    }
    // Fallback: treat entity as a single unit-radius circle.
    return {scale * 0.5f, scale * 0.5f};
}

// ---------------------------------------------------------------------------

TileSet TileGrid::buildOccupancy(const entt::registry& registry,
                                  float tileSize,
                                  entt::entity excludeEntity) {
    TileSet occupied;

    auto view = registry.view<const EditorPlacedComponent,
                               const TransformComponent>();
    for (auto entity : view) {
        if (entity == excludeEntity) continue;

        const auto& epc = view.get<const EditorPlacedComponent>(entity);
        const auto& tc  = view.get<const TransformComponent>(entity);

        glm::vec2 fp = prefabFootprint(epc.prefabAlias, tc.scale);
        TileSet tiles = footprintTiles(tc.position.x, tc.position.z,
                                       fp.x, fp.y, tileSize);
        for (const auto& t : tiles) {
            occupied.insert(t);
        }
    }
    return occupied;
}

// ---------------------------------------------------------------------------

bool TileGrid::isPlacementValid(const entt::registry& registry,
                                 const glm::vec3& ghostPos,
                                 float halfX, float halfZ,
                                 entt::entity excludeEntity) {
    // Guard against non-finite half-extents.
    if (!std::isfinite(halfX) || !std::isfinite(halfZ) ||
        halfX <= 0.0f || halfZ <= 0.0f) {
        return false; // Degenerate footprint — block placement.
    }

    // Use a small epsilon to avoid exact-edge false positives.
    constexpr float kEps = 0.01f;

    auto view = registry.view<const EditorPlacedComponent,
                               const TransformComponent>();
    for (auto entity : view) {
        if (entity == excludeEntity) continue;

        const auto& epc = view.get<const EditorPlacedComponent>(entity);
        const auto& tc  = view.get<const TransformComponent>(entity);

        glm::vec2 fp = prefabFootprint(epc.prefabAlias, tc.scale);

        // XZ AABB overlap test.
        bool overlapX = std::abs(ghostPos.x - tc.position.x) < (halfX + fp.x - kEps);
        bool overlapZ = std::abs(ghostPos.z - tc.position.z) < (halfZ + fp.y - kEps);
        if (overlapX && overlapZ) {
            return false; // Overlap detected — placement is invalid.
        }
    }
    return true;
}

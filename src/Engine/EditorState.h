// src/Engine/EditorState.h
// Global editor mode state — toggled by pressing ~ (tilde / grave accent).
// Shared between InputDispatcher, InputSystem, and EditorSystem.

#ifndef ENGINE_EDITOR_STATE_H
#define ENGINE_EDITOR_STATE_H

#include <string>
#include <set>
#include <utility>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

struct EditorState {
    /// Master toggle — true while the World Editor overlay is open.
    bool isEditorMode = false;

    // --- Prefab / ghost preview ---
    std::string selectedPrefab;         ///< Prefab ID chosen from the dropdown.
    bool        hasGhostEntity  = false;///< Whether a ghost preview is active.
    glm::vec3   ghostPosition   {0.0f}; ///< Current ghost world position (terrain hit).
    float       ghostScale      = 1.0f; ///< Ghost preview scale.
    float       ghostRotationY  = 0.0f; ///< Ghost Y-axis rotation (degrees).

    // --- God-mode camera state saved/restored on toggle ---
    glm::vec3 savedCameraPos   {0.0f};
    float     savedCameraYaw   = 0.0f;
    float     savedCameraPitch = 0.0f;

    // --- Object selection ---
    entt::entity selectedEntity = entt::null; ///< Currently selected placed entity.

    // --- Internal: tilde edge-detection ---
    bool prevTildeDown = false;

    // --- Tile placement grid ---
    /// World-space width/depth of each placement tile in meters.
    /// Objects are snapped to tile centres; only one object per tile is allowed.
    float tileSize = 4.0f;

    /// Integer tile coordinates of the tile currently under the ghost preview.
    int ghostTileX = 0;
    int ghostTileZ = 0;

    /// True when the ghost preview sits on a tile that is already occupied.
    /// When true placement is blocked and the tile grid shows it in red.
    bool ghostOnOccupiedTile = false;

    /// Set of (tileX, tileZ) pairs that are occupied by a placed entity.
    std::set<std::pair<int,int>> occupiedTiles;
};

#endif // ENGINE_EDITOR_STATE_H

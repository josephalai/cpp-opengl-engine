// src/Engine/EditorState.h
// Global editor mode state — toggled by pressing ~ (tilde / grave accent).
// Shared between InputDispatcher, InputSystem, and EditorSystem.

#ifndef ENGINE_EDITOR_STATE_H
#define ENGINE_EDITOR_STATE_H

#include <string>
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
    float     tileSize         = 4.0f;   ///< World-space side length of each tile cell (metres).
    bool      snapToGrid       = true;   ///< Snap ghost position to the nearest tile boundary.
    bool      showTileGrid     = true;   ///< Render tile grid overlay while in editor mode.
    bool      placementBlocked = false;  ///< True when the ghost position would cause an AABB overlap.
    glm::vec2 ghostHalfExtents {0.5f, 0.5f}; ///< XZ half-extents of the ghost entity footprint (× ghostScale). Updated every frame by EditorSystem so TileGridRenderer can highlight the full multi-tile footprint.
};

#endif // ENGINE_EDITOR_STATE_H

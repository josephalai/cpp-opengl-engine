// src/ECS/Components/EditorPlacedComponent.h
// Tag component marking entities placed by the World Editor (vs. gameplay /
// streaming entities).
//
// Entity lifecycle categories in this engine:
//   • Gameplay entities  — player, NPCs, physics objects (loaded from scene.json)
//   • Streaming entities — baked chunk entities loaded at runtime by ChunkManager
//   • Editor entities    — WYSIWYG-placed via the World Editor (tagged here)
//
// Only entities with this component are serialised by EditorSerializer to the
// "editor_entities" array in scene.json.  They can coexist with gameplay and
// streaming entities at runtime.

#ifndef ECS_EDITOR_PLACED_COMPONENT_H
#define ECS_EDITOR_PLACED_COMPONENT_H

#include <string>

struct EditorPlacedComponent {
    std::string prefabAlias;   ///< Prefab ID used to create this entity.
    float       rotationY = 0.0f; ///< Y-axis rotation (degrees) at placement time.
};

#endif // ECS_EDITOR_PLACED_COMPONENT_H

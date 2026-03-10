// src/Engine/EditorSerializer.h
// Serialises editor-placed entities (those with EditorPlacedComponent) to
// src/Resources/scene.json, preserving all non-entity data already in that
// file (terrains, lights, models, physics_bodies, etc.).

#ifndef ENGINE_EDITOR_SERIALIZER_H
#define ENGINE_EDITOR_SERIALIZER_H

#include <string>
#include <entt/entt.hpp>

class EditorSerializer {
public:
    /// Write all entities that carry an EditorPlacedComponent into the
    /// "editor_entities" array inside @p outputPath (scene.json).
    /// Existing keys in the file are preserved; only "editor_entities" is
    /// replaced with the current ECS state.
    /// @return true on success, false if the file could not be written.
    static bool saveToJson(const std::string& outputPath,
                           entt::registry&    registry);
};

#endif // ENGINE_EDITOR_SERIALIZER_H

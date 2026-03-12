// src/ECS/Components/InteractableComponent.h
//
// Attached to world objects (trees, NPCs, rocks) to mark them as interactable.
// Contains the Lua script path and the required interaction range.
// Populated by EntityFactory when the prefab JSON contains an
// "InteractableComponent" block.

#ifndef ENGINE_INTERACTABLE_COMPONENT_H
#define ENGINE_INTERACTABLE_COMPONENT_H

#include <string>

/// Marks an entity as interactable and binds it to a Lua interaction script.
struct InteractableComponent {
    std::string scriptPath;        ///< Relative path to the Lua script, e.g. "scripts/skills/woodcutting.lua"
    float       interactRange = 1.5f; ///< Distance (metres) at which the player can begin the interaction.
};

#endif // ENGINE_INTERACTABLE_COMPONENT_H

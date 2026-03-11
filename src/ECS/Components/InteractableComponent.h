// src/ECS/Components/InteractableComponent.h
//
// Attached to world objects (trees, NPCs, rocks) via EntityFactory when the
// prefab JSON contains an "InteractableComponent" block.
//
// Example prefab JSON:
//   "InteractableComponent": {
//     "script": "scripts/skills/woodcutting.lua",
//     "interact_range": 1.5
//   }

#ifndef ECS_INTERACTABLECOMPONENT_H
#define ECS_INTERACTABLECOMPONENT_H

#include <string>

/// Marks an entity as interactable and points to the Lua script that defines
/// what happens when a player interacts with it.  The C++ InteractionSystem
/// is completely "blind" to what the script does — it only manages distance,
/// timers, and invocation.
struct InteractableComponent {
    std::string scriptPath;          ///< Relative path to the .lua interaction script.
    float       interactRange = 1.5f; ///< Player must be within this radius (metres) to interact.
};

#endif // ECS_INTERACTABLECOMPONENT_H

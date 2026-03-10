// src/ECS/Components/PathfindingComponent.h
//
// Phase 4 Step 4.4.3 — Holds A* / NavMesh waypoints for auto-steering.
// When present on an entity, the server's PathfindingSystem takes control
// of the entity's movement (overriding WASD input).  Removing this
// component returns control to the player (Step 4.4.4 — WASD Interrupt).

#ifndef ECS_PATHFINDINGCOMPONENT_H
#define ECS_PATHFINDINGCOMPONENT_H

#include <vector>
#include <glm/glm.hpp>

struct PathfindingComponent {
    std::vector<glm::vec3> waypoints;       ///< Ordered path from current pos to target.
    int                    currentWaypoint = 0; ///< Index of the next waypoint to reach.
    float                  arrivalRadius   = 1.0f; ///< Distance to consider waypoint reached.
    bool                   active          = true;  ///< Set false to pause auto-steering.
};

#endif // ECS_PATHFINDINGCOMPONENT_H

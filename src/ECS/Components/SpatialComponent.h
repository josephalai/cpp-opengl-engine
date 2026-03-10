// src/ECS/Components/SpatialComponent.h
//
// Phase 4 Step 4.1.2 — ECS component tracking which SpatialGrid cell an
// entity currently occupies.  Attached to every networked entity so the
// spatial-partitioning system can detect cell transitions and migrate
// entities between SpatialCells without a full-grid rescan.

#ifndef ECS_SPATIALCOMPONENT_H
#define ECS_SPATIALCOMPONENT_H

#include <cstdint>

struct SpatialComponent {
    int32_t currentCellX = 0;  ///< Grid column (cellX = floor(pos.x / cellSize)).
    int32_t currentCellZ = 0;  ///< Grid row    (cellZ = floor(pos.z / cellSize)).
};

#endif // ECS_SPATIALCOMPONENT_H

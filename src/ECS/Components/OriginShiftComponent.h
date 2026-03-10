// src/ECS/Components/OriginShiftComponent.h
//
// Phase 4 Step 4.2.4 — Server-side high-precision coordinate storage.
// Uses a ChunkIndex plus a local float offset so the server can track
// entities across an effectively infinite world without float-precision
// degradation.  The client uses a separate origin-shift mechanism
// (resetting the camera to (0,0,0) and translating all local entities).

#ifndef ECS_ORIGINSHIFTCOMPONENT_H
#define ECS_ORIGINSHIFTCOMPONENT_H

#include <glm/glm.hpp>

struct OriginShiftComponent {
    int     chunkIndexX = 0;           ///< Chunk grid X (each chunk = terrainSize m).
    int     chunkIndexZ = 0;           ///< Chunk grid Z.
    glm::vec3 localOffset = {};        ///< Sub-chunk offset (0..terrainSize on each axis).

    /// The absolute world position in double precision, reconstructed on demand.
    double absoluteX(float chunkSize) const {
        return static_cast<double>(chunkIndexX) * static_cast<double>(chunkSize)
             + static_cast<double>(localOffset.x);
    }
    double absoluteZ(float chunkSize) const {
        return static_cast<double>(chunkIndexZ) * static_cast<double>(chunkSize)
             + static_cast<double>(localOffset.z);
    }
};

#endif // ECS_ORIGINSHIFTCOMPONENT_H

// src/ECS/Components/LODComponent.h
//
// Phase 4 Step 4.3.1 — Component that stores per-entity LOD state.
// The RenderSystem uses the current LOD level to select the appropriate
// mesh variation (LOD0 = high poly, LOD1 = mid, LOD2 = low).

#ifndef ECS_LODCOMPONENT_H
#define ECS_LODCOMPONENT_H

#include <cstdint>

struct LODComponent {
    uint8_t currentLOD  = 0;    ///< 0 = highest detail, 2 = lowest.
    float   lodDistance0 = 20.0f;  ///< Below this: LOD0 (high poly).
    float   lodDistance1 = 60.0f;  ///< Below this: LOD1 (mid poly); above: LOD2.
};

#endif // ECS_LODCOMPONENT_H

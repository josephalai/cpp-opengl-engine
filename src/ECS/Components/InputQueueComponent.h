// src/ECS/Components/InputQueueComponent.h
//
// [Phase 3.3] Server-side input queue used by the authoritative simulation.
//
// The RECEIVE handler in ServerMain.cpp appends each incoming PlayerInputPacket
// to this component's queue instead of applying movement immediately.  During
// the fixed-rate tick loop, a view over all entities with InputQueueComponent
// drains the queue through SharedMovement::applyInput() — giving the server
// full authority over every entity's position.
//
// This component is only *used* by the headless server, but the header is safe
// to include in any translation unit (no GL/GLFW dependencies, lives in
// src/ECS/Components/ which is part of COMMON_SOURCES).

#ifndef ECS_INPUTQUEUECOMPONENT_H
#define ECS_INPUTQUEUECOMPONENT_H

#include <vector>
#include "../../Network/NetworkPackets.h"

/// Buffers incoming PlayerInputPackets for an entity until the next server tick.
/// Drained and cleared during the authoritative movement step.
///
/// Vertical physics state (upwardsSpeed, isInAir) is stored here so that
/// gravity accumulation and jump arcs persist smoothly across ticks.
/// SharedMovement::applyInput (full overload) owns these fields exclusively;
/// Bullet is only used for horizontal (XZ) wall-sliding.
struct InputQueueComponent {
    std::vector<Network::PlayerInputPacket> inputs;

    /// Authoritative vertical velocity (m/s). Integrates gravity each input.
    float upwardsSpeed = 0.0f;
    /// True when the entity is airborne (between a jump and landing).
    bool  isInAir      = false;
};

#endif // ECS_INPUTQUEUECOMPONENT_H

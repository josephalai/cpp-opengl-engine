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
/// Also stores the persistent vertical-movement state (upwardsSpeed, isInAir) so
/// gravity and jump arcs survive across ticks — matching the client's
/// InputStateComponent fields exactly.
struct InputQueueComponent {
    std::vector<Network::PlayerInputPacket> inputs;

    /// Vertical velocity (m/s). Positive = moving up. Persistent across ticks.
    float upwardsSpeed = 0.0f;
    /// True while the entity is airborne (no terrain contact).
    bool  isInAir      = false;
};

#endif // ECS_INPUTQUEUECOMPONENT_H

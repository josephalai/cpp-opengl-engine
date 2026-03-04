// src/Events/Event.h
// Base Event type and concrete engine events for the EventBus pub/sub system.
//
// New event types should inherit from Event.  The EventBus dispatches by
// std::type_index, so no runtime RTTI enum or virtual dispatch is needed for
// routing — only the virtual destructor is provided for safe polymorphic
// deletion in case events are ever heap-allocated.

#ifndef ENGINE_EVENT_H
#define ENGINE_EVENT_H

#include <glm/glm.hpp>

/// Base type for all engine events.
struct Event {
    virtual ~Event() = default;
};

// ---------------------------------------------------------------------------
// Input-derived command events
// ---------------------------------------------------------------------------

/// Published by InputDispatcher each frame when movement keys are polled.
/// Consumers (Player, PhysicsSystem, networking) subscribe to this event
/// instead of calling InputMaster::isKeyDown() directly in their update loops.
struct PlayerMoveCommandEvent : Event {
    float forward;      ///< +1 = forward,  -1 = backward,  0 = none
    float turn;         ///< +1 = turn left, -1 = turn right, 0 = none
    bool  jump;         ///< Space held this frame
    bool  sprint;       ///< Tab held — activates speed boost
    bool  sprintReset;  ///< Backslash held — resets sprint speed
};

/// Published by InputDispatcher on the rising edge of a right-mouse-button
/// press when the terrain picker resolves a valid world-space intersection.
/// Replaces direct calls to TerrainPicker::getCurrentTerrainPoint() inside
/// gameplay systems.
struct TargetLocationClickedEvent : Event {
    glm::vec3 worldPosition; ///< 3-D terrain intersection point
};

// ---------------------------------------------------------------------------
// World / streaming events
// ---------------------------------------------------------------------------

/// Published by ChunkManager (or StreamingSystem) when a streaming chunk
/// transitions between loaded and unloaded states.
struct ChunkLoadStatusEvent : Event {
    enum class Status { Loaded, Unloaded };
    Status status;
    int chunkX;
    int chunkZ;
};

// ---------------------------------------------------------------------------
// Display events
// ---------------------------------------------------------------------------

/// Published when the GLFW framebuffer is resized.
struct WindowResizeEvent : Event {
    int width;
    int height;
};

#endif // ENGINE_EVENT_H

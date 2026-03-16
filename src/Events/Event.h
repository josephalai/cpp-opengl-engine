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
#include <cstdint>
#include <string>

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
    float forward;    ///< +1 = forward,  -1 = backward,  0 = none
    float strafe;     ///< +1 = strafe left, -1 = strafe right, 0 = none (camera-relative)
    float cameraYaw;  ///< Absolute camera orbit yaw (degrees) — movement reference direction
    bool  jump;       ///< Space held this frame
    bool  sprint;     ///< Tab held — activates speed boost
    bool  sprintReset;///< Backslash held — resets sprint speed
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

// ---------------------------------------------------------------------------
// Skilling / progression events
// ---------------------------------------------------------------------------

/// Published by the client NetworkSystem when an InventorySyncPacket arrives,
/// or locally when items are added/removed.  UI systems subscribe to refresh
/// the inventory grid without coupling to network code.
struct InventoryUpdatedEvent : Event {};

/// Published by the client NetworkSystem when a SkillsSyncPacket arrives.
/// The SkillsPanel subscribes to refresh XP bars and level labels.
struct SkillsSyncReceivedEvent : Event {};

/// Published (client-side) when the player gains XP in a skill.
/// Decouples the skilling Lua script result from the SkillsPanel renderer.
struct XpGainedEvent : Event {
    int   skillId  = 0;   ///< Index into SkillsComponent::xp array (see SkillId enum).
    int   amount   = 0;   ///< Raw XP awarded this action.
    int   newTotal = 0;   ///< XP total after the gain.
};

// ---------------------------------------------------------------------------
// Chat events
// ---------------------------------------------------------------------------

/// Published by the client NetworkSystem when a ChatMessagePacket arrives.
/// The ChatBox subscribes to append the message to its scroll buffer.
struct ChatReceivedEvent : Event {
    uint32_t    senderNetworkId = 0;          ///< Originating player's network ID.
    std::string senderName;                   ///< Display name (resolved from NetworkId).
    std::string message;                      ///< UTF-8 chat message body.
};

// ---------------------------------------------------------------------------
// Context-menu / interaction events
// ---------------------------------------------------------------------------

/// Published when the player selects an option from the OSRS-style right-click
/// context menu.  The NetworkSystem subscribes and forwards an ActionRequestPacket.
struct ContextMenuActionEvent : Event {
    uint32_t targetNetworkId = 0;  ///< Entity the player right-clicked.
    int      actionIndex     = 0;  ///< Index into the entity's prefab "actions" array.
    std::string actionName;        ///< Human-readable label (e.g. "Chop down", "Talk").
};

#endif // ENGINE_EVENT_H

// src/Events/EntityClickedEvent.h
//
// Published by the client-side EntityPicker / InputDispatcher when the
// player right-clicks on a world entity that has a NetworkIdComponent.
//
// The NetworkSystem listens for this event and sends an ActionRequestPacket
// to the server containing the target's network ID.

#ifndef ENGINE_ENTITY_CLICKED_EVENT_H
#define ENGINE_ENTITY_CLICKED_EVENT_H

#include <cstdint>

/// Fired when the player right-clicks a world entity.
/// The NetworkSystem translates this into an ActionRequestPacket.
struct EntityClickedEvent {
    uint32_t networkId = 0; ///< The NetworkIdComponent::id of the clicked entity.
};

#endif // ENGINE_ENTITY_CLICKED_EVENT_H

#ifndef ECS_NETWORKIDCOMPONENT_H
#define ECS_NETWORKIDCOMPONENT_H

#include <cstdint>
#include <string>

/// POD component that associates an entt::entity with its wire-format network ID
/// and per-entity metadata needed for spawn/despawn/snapshot packets.
/// Used by both the headless server (authoritative simulation) and the client
/// (remote entity representation).
struct NetworkIdComponent {
    uint32_t    id            = 0;          ///< Unique network ID assigned at connect time.
    std::string modelType     = "player";   ///< Model prefab key sent in SpawnPacket.
    bool        isNPC         = false;      ///< True for server-controlled NPCs.
    uint32_t    lastInputSeq  = 0;          ///< Last processed client input sequence number.
};

#endif // ECS_NETWORKIDCOMPONENT_H

// src/ECS/Components/ActiveChunkTag.h
//
// Empty tag component added to registry entities that are inside an active
// streaming chunk.  Systems include this tag in their view<> queries to
// naturally cull entities outside the loaded region without needing
// external side-vectors.
//
// StreamingSystem::update() is the sole writer:
//   - registry.emplace_or_replace<ActiveChunkTag>(e)  when entity enters an active chunk
//   - registry.remove<ActiveChunkTag>(e)              when entity leaves all active chunks
//
// Entities that are always visible (e.g. when there is no ChunkManager) are
// tagged once in Engine::buildSystems() and never untagged.

#ifndef ECS_ACTIVECHUNKTAG_H
#define ECS_ACTIVECHUNKTAG_H

/// Tag component — no data, used only for registry view filtering.
struct ActiveChunkTag {};

#endif // ECS_ACTIVECHUNKTAG_H

// src/ECS/Components/TerrainComponent.h
//
// EnTT component that stores a raw (non-owning) pointer to a heap-allocated
// Terrain tile.  One registry entity is created per terrain tile loaded by
// SceneLoader/SceneLoaderJson.
//
// Ownership: the Terrain* is allocated by the Loader and owned by the Terrain
// object itself (Terrain manages its own GPU resources).  Currently the pointer
// is not explicitly freed on shutdown (same pre-migration behaviour).
//
// ActiveChunkTag is added/removed by StreamingSystem to mark which terrain
// tiles are inside active streaming chunks.  RenderSystem queries
//   registry.view<TerrainComponent, ActiveChunkTag>()
// to build the per-frame visible terrain list.

#ifndef ECS_TERRAINCOMPONENT_H
#define ECS_TERRAINCOMPONENT_H

class Terrain;

struct TerrainComponent {
    Terrain* terrain = nullptr; ///< non-owning pointer to the terrain tile
};

#endif // ECS_TERRAINCOMPONENT_H

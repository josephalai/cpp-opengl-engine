// src/Engine/SceneLoaderJson.h
//
// JSON-based scene loader (Phase 1, Step 3 — Strict Data-Driven Design).
//
// Reads a scene.json file and populates the engine scene. Static geometry
// from the "entities" array is emitted as ECS entities with StaticModelComponent
// (no legacy Entity* vector). Animated characters use AnimatedModelComponent.
// The only legacy Entity* output is the Player.

#ifndef ENGINE_SCENELOADERJSON_H
#define ENGINE_SCENELOADERJSON_H

#include <string>
#include <vector>

#include "../RenderEngine/Loader.h"
#include "../Entities/Player.h"
#include "../Entities/PlayerCamera.h"
#include "../Entities/Light.h"
#include "../Terrain/Terrain.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Guis/Text/GUIText.h"
#include "../Water/WaterTile.h"
#include "../Physics/PhysicsComponents.h"
#include "SceneLoader.h"   // re-use PhysicsBodyCfg / PhysicsGroundCfg types
#include <entt/entt.hpp>

class SceneLoaderJson {
public:
    // Uses the same PhysicsBodyCfg / PhysicsGroundCfg types as the legacy
    // SceneLoader so the Engine can call either loader transparently.
    using PhysicsBodyCfg  = SceneLoader::PhysicsBodyCfg;
    using PhysicsGroundCfg = SceneLoader::PhysicsGroundCfg;

    /// Load a scene from a JSON file.  Returns true on success.
    /// Falls back gracefully if the file is missing; the caller should then
    /// use the legacy SceneLoader::load() with the .cfg path.
    ///
    /// Static mesh entities → ECS StaticModelComponent + TransformComponent
    /// Animated characters  → ECS AnimatedModelComponent + TransformComponent
    /// Player               → Player* (placed in `player` out-param)
    /// No legacy std::vector<Entity*> is populated or needed.
    static bool load(
        const std::string&             jsonPath,
        Loader*                        loader,
        entt::registry&                registry,
        std::vector<Light*>&           lights,
        std::vector<Terrain*>&         allTerrains,
        std::vector<GuiTexture*>&      guis,
        std::vector<GUIText*>&         texts,
        std::vector<WaterTile>&        waterTiles,
        Terrain*&                      primaryTerrain,
        Player*&                       player,
        PlayerCamera*&                 playerCamera,
        std::vector<PhysicsBodyCfg>&   physicsBodyCfgs,
        std::vector<PhysicsGroundCfg>& physicsGroundCfgs
    );
};

#endif // ENGINE_SCENELOADERJSON_H

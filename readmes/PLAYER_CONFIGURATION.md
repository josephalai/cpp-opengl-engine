# Player Configuration

This document explains how to configure the main player character in the engine. The player
can be loaded in two modes:

- **Legacy alias mode** — a static OBJ/textured model referenced by a model alias already
  defined in the scene file.
- **Prefab mode** — the player is spawned via the `EntityFactory`/`PrefabManager` pipeline,
  exactly like NPCs and animated characters. This supports static meshes (Assimp), monolithic
  animated `.glb` files, and modular animated `.glb` files (separate skin + animation clips).

---

## Table of Contents

1. [scene.json Configuration](#scenejson-configuration)
   - [Legacy static OBJ model (alias)](#1-legacy-static-obj-model-alias)
   - [Prefab — monolithic animated .glb](#2-prefab--monolithic-animated-glb)
   - [Prefab — modular animated .glb](#3-prefab--modular-animated-glb)
2. [scene.cfg Configuration](#scenecfg-configuration)
3. [Prefab JSON Format for the Player](#prefab-json-format-for-the-player)
   - [Monolithic variant](#monolithic-variant)
   - [Modular variant](#modular-variant)
4. [How It Works Internally](#how-it-works-internally)
5. [Troubleshooting](#troubleshooting)

---

## scene.json Configuration

The `"player"` block sits at the top level of `scene.json`.

### 1. Legacy static OBJ model (alias)

Use this when the player mesh is a `.obj` file registered earlier in the `"models"` array.

```json
{
  "models": [
    {
      "alias": "player_model",
      "obj":   "Resources/Models/player.obj",
      "texture": "Resources/Textures/player.png"
    }
  ],
  "player": {
    "alias": "player_model",
    "x": 100.0,
    "y": 0.0,
    "z": -50.0,
    "rx": 0.0,
    "ry": 180.0,
    "rz": 0.0,
    "scale": 1.0
  }
}
```

The `"y"` value can be a float **or** the special string `"terrain"` / `"terrain+<offset>"` to
snap the player to the terrain height at spawn:

```json
"y": "terrain+1.8"
```

### 2. Prefab — monolithic animated .glb

Use this when the player character is a single `.glb` that contains both the skeleton and all
animation clips embedded.

```json
{
  "player": {
    "prefab": "player_character",
    "x": 100.0,
    "y": "terrain",
    "z": -50.0,
    "scale": 1.0
  }
}
```

The `"prefab"` value must match the filename (without extension) of a JSON prefab registered
in `Resources/prefabs/player_character.json`.

### 3. Prefab — modular animated .glb

Use this when the skin and animation clips are stored in separate `.glb` files.

```json
{
  "player": {
    "prefab": "player_character",
    "x": 100.0,
    "y": "terrain+1.8",
    "z": -50.0,
    "ry": 180.0,
    "scale": 1.0
  }
}
```

The prefab JSON selects the modular path automatically when it contains an
`AnimationControllerComponent` with an `animations` map (see below).

---

## scene.cfg Configuration

The `.cfg` player line syntax is:

```
player <alias> <x> <y> <z> [rx ry rz] [scale=F] [prefab=<id>]
```

### Legacy alias only

```
player player_model 100 0 -50 0 180 0 scale=1.0
```

### With prefab

```
player player_model 100 0 -50 0 180 0 scale=1.0 prefab=player_character
```

When `prefab=<id>` is present **and** the prefab is registered in `PrefabManager`, the engine
uses `EntityFactory::spawn()` to create the animated entity. The `<alias>` token is still
parsed (and used as a fallback OBJ model lookup), but rendering comes from the ECS
`AnimatedModelComponent`.

---

## Prefab JSON Format for the Player

Prefab files live in `Resources/prefabs/<id>.json`.

### Monolithic variant

The `.glb` file contains the skeleton + all animations in one file. No
`AnimationControllerComponent` is required (the engine detects the absence of external clips
and uses `AnimationLoader::load()` directly).

```json
{
  "id": "player_character",
  "animated": true,
  "components": {
    "AnimatedModelComponent": {
      "model_path": "Resources/Models/Characters/player.glb"
    }
  },
  "animation_map": {
    "idle":   0,
    "walk":   1,
    "run":    2,
    "attack": 3
  },
  "default_state": "idle"
}
```

> **Note:** The integer values in `animation_map` are zero-based indices into the animation
> list embedded in the `.glb` file, in the order they appear. Use a tool such as
> [gltf-transform](https://gltf-transform.donmccurdy.com/) or Blender's glTF exporter
> export log to confirm the animation order.

### Modular variant

The skin (rest-pose skeleton + mesh) is in one `.glb` and each animation clip is a separate
`.glb`. The `AnimationControllerComponent` lists the clips to load.

```json
{
  "id": "player_character",
  "animated": true,
  "components": {
    "AnimatedModelComponent": {
      "model_path": "Resources/Models/Characters/player_skin.glb"
    },
    "AnimationControllerComponent": {
      "default_state": "idle",
      "animations": {
        "idle":   "Resources/Animations/player_idle.glb",
        "walk":   "Resources/Animations/player_walk.glb",
        "run":    "Resources/Animations/player_run.glb",
        "attack": "Resources/Animations/player_attack.glb"
      }
    }
  }
}
```

---

## How It Works Internally

### Prefab detection

When the scene loader encounters a non-empty `"prefab"` key (JSON) or `prefab=<id>` token
(CFG) it checks `PrefabManager::get().hasPrefab(id)`. If the prefab exists, it calls
`EntityFactory::spawn(registry, id, position, nullptr, rotation, scale)`.

### EntityFactory::spawn()

`EntityFactory::spawn()` reads the prefab JSON and:

1. Creates an ECS entity and emplaces `TransformComponent`.
2. **Modular path** — if the prefab has an `AnimationControllerComponent` with a non-empty
   `animations` map, it calls `AnimationLoader::loadSkin()` for the mesh and
   `AnimationLoader::loadExternalAnimation()` for each clip. Each clip is registered as a
   named state in the `AnimationController`.
3. **Monolithic path** — if no `AnimationControllerComponent` (or empty `animations`), it
   calls `AnimationLoader::load()` on the single `.glb` and maps animation indices to state
   names via `animation_map`.
4. **Static path** — if neither an `AnimatedModelComponent` nor `AnimationControllerComponent`
   is present, an `AssimpModelComponent` is created instead.

### Player* wrapper

After `EntityFactory::spawn()` the scene loader still creates a `Player*` object (a thin
`Entity` subclass). This wrapper exists for legacy `Engine` code that accesses
`player->getPosition()`, sets up `PlayerCamera`, and wires `InputStateComponent`. The actual
character rendering is driven by the ECS `AnimatedModelComponent` on the spawned entity, which
is marked `isLocalPlayer = true`.

The `Player*` entity's `TexturedModel*` may be `nullptr` when no OBJ alias matches — this is
intentional and safe: `MasterRenderer::processEntity` skips entities with a null model.

---

## Troubleshooting

### Prefab not found

```
[SceneLoaderJson] player references unknown alias '<id>'
```

or no player spawns at all. Check:

- The file `Resources/prefabs/<id>.json` exists and is valid JSON.
- `PrefabManager` was initialised before `SceneLoaderJson::load()` is called.
- The `"id"` field inside the prefab JSON matches the filename (without `.json`).

### Player invisible (prefab path)

- Confirm the `AnimatedModelComponent::isLocalPlayer` flag is `true` on the spawned entity
  (`Engine::loadScene` marks all AMC entities after load, but the scene loader also sets it
  explicitly).
- Check the `.glb` path in the prefab JSON is correct relative to the working directory.
- If the mesh has no embedded texture, the `AnimatedRenderer` automatically binds a 1×1 white
  fallback texture so the fragment-shader alpha discard does not hide the mesh.

### Animation channels not matching

```
[AnimationLoader] bone 'X' not found in skeleton — channel skipped
```

The bone names in the animation `.glb` must match the bone names in the skin `.glb` exactly.
Ensure both files were exported from the same armature in Blender.

### Draco-compressed .glb not loading

The engine's Assimp build must include the Draco decoder. Export as a standard (non-Draco)
`.glb` from Blender, or rebuild Assimp with `-DASSIMP_BUILD_DRACO=ON`.

### Camera does not follow the player

If `playerCamera` is `nullptr` after loading, `Engine::loadScene` creates a fallback camera.
Verify the prefab ID is spelled correctly and `PrefabManager` has been populated before the
scene loads.

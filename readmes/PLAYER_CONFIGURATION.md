# Player Configuration

This document explains how to configure the main player character in the engine.
The `"player"` block in `scene.json` is the **single source of truth** for the player
character — it defines both the visual model and the spawn position/rotation/scale.

There are three loading modes, tried in priority order:

| Priority | Key present | Mode |
|----------|-------------|------|
| 1 | `"path"` | **Animated model** — monolithic `.glb` with embedded skeleton + clips |
| 2 | `"prefab"` | **Prefab** — `EntityFactory::spawn()` (modular or monolithic `.glb` via `PrefabManager`) |
| 3 | `"alias"` | **Legacy static OBJ** — look up a pre-loaded OBJ model by alias |

---

## Table of Contents

1. [scene.json — Animated model (recommended)](#1-scenejson--animated-model-recommended)
2. [scene.json — Prefab pipeline](#2-scenejson--prefab-pipeline)
3. [scene.json — Legacy static OBJ (alias)](#3-scenejson--legacy-static-obj-alias)
4. [scene.cfg — Player line](#4-scenecfg--player-line)
5. [How it works internally](#5-how-it-works-internally)
6. [Troubleshooting](#6-troubleshooting)

---

## 1. scene.json — Animated model (recommended)

Use when the player character is a **monolithic `.glb`** containing the skeleton and all
animation clips in a single file. This is the primary mode used by `scene.json` today.

```json
"player": {
  "path": "Characters/man-1.glb",
  "fallback_path": "Characters/characters.glb",
  "animation_map": {
    "Idle":   "Idle",
    "Walk":   "Casual_Walk",
    "Run":    "Running",
    "Sprint": "Run_03",
    "Patrol": "Walking"
  },
  "offset": { "ox": 0.0, "oy": 0.0, "oz": 0.0 },
  "rot":    { "rx": 0.0, "ry": 0.0, "rz": 0.0 },
  "scale": 0.05,
  "x": 100.0,
  "y": "terrain",
  "z": -80.0
}
```

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `path` | string | Path to the `.glb` relative to `src/Resources/Models/` |
| `fallback_path` | string (optional) | Secondary `.glb` loaded if `path` fails. When the fallback is used, `animation_map` is ignored and clip names are auto-detected. |
| `animation_map` | object (optional) | Maps state names → clip names (case-sensitive). If omitted, clips are auto-detected via `normalizeClipName()`. |
| `offset` | object (optional) | Visual-only model offset `{ox, oy, oz}` in world units. Shifts the rendered mesh without moving the physics capsule. |
| `rot` | object (optional) | Model-space rotation correction `{rx, ry, rz}` in degrees. Baked into the coordinate correction matrix. Use to fix Z-up or mis-oriented exports. |
| `scale` | float (optional, default `1.0`) | Uniform scale applied to both the visual mesh and physics capsule. |
| `x`, `y`, `z` | float / string | World spawn position. `"y"` supports `"terrain"` or `"terrain+<offset>"` to snap to the heightmap. |
| `rx`, `ry`, `rz` | float (optional, default `0`) | World-space initial rotation for the `Player*` entity (camera/physics wrapper). Typically left at zero; animation facing is driven by movement input. |

### `animation_map` note

The left-hand key is the **state name** used by `AnimationController` and
`AnimationSystem` (e.g. `"Idle"`, `"Walk"`, `"Run"`). The right-hand value is the
**exact clip name** as embedded in the `.glb` (case-sensitive). Use a tool such as
[gltf-transform](https://gltf-transform.donmccurdy.com/) or check the loader log at
startup to see the exact clip names your file contains.

---

## 2. scene.json — Prefab pipeline

Use when the player character is managed via the `PrefabManager` + `EntityFactory` system.
This supports both monolithic `.glb` and modular `.glb` (separate skin + animation clip files).

```json
"player": {
  "prefab": "player_character",
  "x": 100.0,
  "y": "terrain",
  "z": -80.0,
  "scale": 1.0
}
```

The `"prefab"` value must match the filename (without `.json`) of a file in
`Resources/prefabs/`. `EntityFactory::spawn()` handles all loading (skin, clips,
`AnimationControllerComponent`) and marks the entity `isLocalPlayer = true`.

A `Player*` wrapper is still created for camera/physics/input compatibility. Its
`TexturedModel*` will be `nullptr` (rendering comes from the ECS
`AnimatedModelComponent`); `MasterRenderer` skips null-model entities safely.

---

## 3. scene.json — Legacy static OBJ (alias)

Use when the player character is an OBJ mesh that has already been registered in the
`"models"` array.

```json
"models": [
  { "alias": "player_model", "obj": "Characters/person", "texture": "playerTexture" }
],
"player": {
  "alias": "player_model",
  "x": 100.0,
  "y": 0.0,
  "z": -80.0,
  "rx": 0.0,
  "ry": 180.0,
  "rz": 0.0,
  "scale": 1.0
}
```

`"y"` supports the `"terrain"` / `"terrain+N"` string syntax here too.

---

## 4. scene.cfg — Player line

The `.cfg` syntax is:

```
player <alias> <x> <y> <z> [rx ry rz] [scale=F] [prefab=<id>]
```

Examples:

```cfg
# Legacy static OBJ
player player_model 100 0 -80 0 180 0 scale=1.0

# Via prefab pipeline
player player_model 100 terrain -80 0 0 0 scale=1.0 prefab=player_character
```

When `prefab=<id>` is present **and** the prefab is registered, `EntityFactory::spawn()`
is used; otherwise the `<alias>` OBJ lookup is the fallback.

> **Note:** The `.cfg` format does not (yet) support direct animated-model loading via
> `path=`. Use the `prefab=` token to load an animated player from a `.cfg` scene.

---

## 5. How it works internally

### Two-entity architecture

The engine keeps **two separate ECS entities** for the local player:

| Entity | Key component | Purpose |
|--------|---------------|---------|
| **Player entity** (`player->getHandle()`) | `InputStateComponent` | Receives keyboard/gamepad input; `PlayerMovementSystem` updates `TransformComponent` position each frame |
| **Animated visual entity** | `AnimatedModelComponent` with `isLocalPlayer = true` | Rendered by `AnimatedRenderer`; `AnimationSystem` syncs its `TransformComponent.position` from `player->getPosition()` every frame and drives the animation state machine |

### `isLocalPlayer` flag

`AnimationSystem` checks `amc.isLocalPlayer` to decide how to drive the entity:

- **`true`** — position/rotation are copied from `Player*->getPosition()` and
  `Player*->getRotation()` every frame. Movement delta drives `Walk`/`Run`/`Idle`
  transitions.
- **`false`** — position is driven by `NetworkInterpolationSystem` (remote/NPC entities).

### Startup marking

After `SceneLoaderJson::load()`, `Engine::loadScene()` applies a backward-compat rule:
if **no** animated entity has already been marked `isLocalPlayer = true` (e.g. a legacy
`.cfg` scene file where the player's animated body came from an `animated_character`
line), **all** animated entities are marked `true`. This preserves old scene files
while allowing explicit per-entity control in the new `"player"` block.

### animated_characters vs player

The `"animated_characters"` array is intended for **NPC** animated meshes. Each entry
creates an `AnimatedModelComponent` with `isLocalPlayer = false`. The player's animated
body should be defined in the `"player"` block using the `"path"` key — not in
`"animated_characters"`.

---

## 6. Troubleshooting

### Player is invisible (animated path)

- Check that `"path"` is correct relative to `src/Resources/Models/`.
- Provide a `"fallback_path"` pointing to a known-good `.glb` for a quick test.
- If the mesh has no embedded texture, `AnimatedRenderer` binds a 1×1 white fallback
  texture automatically — the model should still be visible (white/grey shaded).

### No animations playing

- Check the startup log for lines like `state 'Idle' <- clip 'Idle'`. If the state
  you expect is missing, the clip name in `animation_map` doesn't match what's in the
  file.
- Run `gltf-transform inspect <file>.glb` to list exact clip names.
- If you omit `animation_map`, clips are auto-detected: `idle*/Idle*` → `"Idle"`,
  `walk*` → `"Walk"`, `run*` → `"Run"`, `jump*` → `"Jump"`.

### Player loads at wrong height

- Use `"y": "terrain"` to snap to the terrain surface at spawn, or
  `"y": "terrain+1.8"` to spawn 1.8 units above the surface.

### Player not found / camera at origin

- If `player` is `nullptr` after loading, `Engine::loadScene()` falls back to a stall
  model at a hardcoded position. Check the console for
  `[SceneLoaderJson] Failed to load player model:` or
  `[SceneLoaderJson] player references unknown alias`.

### Prefab not found

- Ensure `Resources/prefabs/<id>.json` exists and is valid JSON.
- The `"id"` field inside the prefab JSON must match the filename (without `.json`).
- `PrefabManager` must be initialised before `SceneLoaderJson::load()` is called.

### Draco-compressed .glb not loading

Export as standard (non-Draco) `.glb` from Blender, or rebuild Assimp with
`-DASSIMP_BUILD_DRACO=ON`.

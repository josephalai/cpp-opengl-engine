# Modular Animation Pipeline — Developer Guide

This document explains how to use the MMO-style modular asset pipeline supported by the engine.  It covers `AnimationLoader`, `AnimationControllerComponent`, the `EntityFactory` JSON prefab format, and the Draco/Assimp build dependency.

---

## Overview

The engine supports two animation-loading modes:

| Mode | Description | When to use |
|------|-------------|-------------|
| **Monolithic** (legacy) | Mesh + skeleton + animations in a single `.glb` file | Existing assets, simple NPCs |
| **Modular** (MMO-style) | Skin `.glb` (mesh + skeleton) + per-animation `.glb` files | Large worlds, shared rigs, memory-efficient streaming |

`AssetForge.py` produces modular assets:

```
Resources/
  skins/
    npc.glb          ← mesh + bind-pose skeleton only
  animations/
    idle.glb         ← skeleton + Idle track only
    walk.glb         ← skeleton + Walk track only
    run.glb          ← skeleton + Run track only
```

---

## Phase 1 — `AnimationLoader` API

### `AnimationLoader::load(path)` — unchanged legacy method

Loads a single `.glb`/`.fbx` file containing mesh, skeleton **and** embedded animation clips.  All clips are stored in `AnimatedModel::clips` and registered with the `AnimationController`.

```cpp
AnimatedModel* model = AnimationLoader::load("Resources/Models/character.glb");
```

### `AnimationLoader::loadSkin(skinPath)` — new

Loads **only** the mesh geometry and bind-pose skeleton.  Any `mAnimations` present in the file are intentionally ignored.

```cpp
AnimatedModel* skin = AnimationLoader::loadSkin("Resources/skins/npc.glb");
// skin->meshes  → GPU-ready meshes
// skin->skeleton → bind-pose bone hierarchy (no clips loaded)
// skin->clips   → empty
```

Returns `nullptr` on failure (Assimp error, missing file, etc.).

### `AnimationLoader::loadExternalAnimation(animPath, targetSkeleton)` — new

Loads the first animation track from an animation-only `.glb` and maps its channels onto `targetSkeleton` by **bone name**.

```cpp
auto clip = AnimationLoader::loadExternalAnimation(
    "Resources/animations/walk.glb",
    &skin->skeleton);
// clip->channels contains only channels whose names matched bones in the skeleton.
// Unmatched channels (e.g. "Armature", "RootMotion") are silently ignored.
```

Returns a `std::shared_ptr<AnimationClip>`, or `nullptr` if no animations were found or the file failed to load.

**Bone-name matching note:** The channel names in the animation file must match the bone names stored in `targetSkeleton`.  Mixamo rigs typically use names like `mixamorig:RightArm`.  The skin and animation files **must be exported from the same rig** for names to align.  If you see a `WARNING: no channels matched` message, check that both files were exported from the same Blender armature.

---

## Phase 2 — `AnimationControllerComponent`

A new ECS component (header: `src/ECS/Components/AnimationControllerComponent.h`) stores the externally-loaded clips produced by the modular pipeline.

```cpp
struct AnimationControllerComponent {
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> animations;
    std::string defaultState;        // from "default_state" in JSON
    std::string currentAnimationName; // mirrors AnimationController state
    float playbackTime = 0.0f;
};
```

The `animations` map keeps the `shared_ptr` owners alive so raw `AnimationClip*` pointers stored inside `AnimationController` remain valid for the entity's lifetime.

> **Note:** This component is attached only in **Modular mode**.  Monolithic entities do not have it — their clips live inside `AnimatedModel::clips`.

---

## Phase 3 — JSON Prefab Format

### Scenario 1 — Monolithic (legacy, unchanged)

No `AnimationController` component, or its `animations` object is absent/empty.  The engine calls `AnimationLoader::load(mesh)` and uses all embedded clips.

```json
{
  "name": "npc_guard",
  "mesh": "models/guard.glb",
  "animated": true,
  "components": {
    "AnimatedModelComponent": {
      "scale": 1.0,
      "animation_map": {
        "Idle": "Guard_Idle",
        "Walk": "Guard_Walk"
      }
    }
  }
}
```

### Scenario 2 — Modular (split-file)

Add a `"AnimationController"` block under `"components"` with a non-empty `"animations"` object.  The engine switches to `loadSkin()` + `loadExternalAnimation()`.

```json
{
  "name": "npc_archer",
  "mesh": "skins/npc.glb",
  "animated": true,
  "components": {
    "AnimatedModelComponent": {
      "mesh_path": "skins/npc.glb",
      "scale": 1.0,
      "model_offset": { "x": 0.0, "y": 0.0, "z": 0.0 }
    },
    "AnimationController": {
      "default_state": "Idle",
      "animations": {
        "Idle": "animations/idle.glb",
        "Walk": "animations/walk.glb",
        "Run":  "animations/run.glb"
      }
    }
  }
}
```

**Fields:**

| Field | Required | Description |
|-------|----------|-------------|
| `mesh` (top-level) | Yes | Path to the skin `.glb`, resolved via `FileSystem::Scene()` |
| `components.AnimatedModelComponent.mesh_path` | No | Alternative path to the skin (overrides top-level `mesh` for the load call) |
| `components.AnimationController.animations` | Yes (modular) | Map of state name → animation `.glb` path |
| `components.AnimationController.default_state` | No | State to activate after loading (defaults to `"Idle"` if present) |

**Path resolution:** All paths are resolved through `FileSystem::Scene()`, which maps relative paths relative to `src/Resources/`.  Example: `"animations/idle.glb"` → `<repo>/src/Resources/animations/idle.glb`.

**Fallback state selection:** If `default_state` is not specified (or not found in `animations`), the engine falls back to:
1. A state named `"Idle"` (if found).
2. The first state added to the controller.

---

## Phase 4 — Draco Assimp Support

### What is Draco?

Google's Draco is a mesh-compression library.  `AssetForge.py` uses Draco by default to shrink skin GLBs below 1 MB.  Standard pre-compiled Assimp **does not** include the Draco decoder unless explicitly built with `-DASSIMP_BUILD_DRACO=ON`.

If Draco-compressed skins are loaded with a non-Draco Assimp build, you will see:

```
[AnimationLoader::loadSkin] Assimp error: GLTF: Draco mesh compression not supported.
```

### Default behaviour (Draco ON)

`CMakeLists.txt` now uses `FetchContent` to build Assimp from source with Draco enabled by default.  The `ENGINE_ASSIMP_WITH_DRACO` option is `ON` out of the box:

```cmake
option(ENGINE_ASSIMP_WITH_DRACO "Build Assimp from source with Draco support" ON)
```

This means:
- The **first** `cmake ..` run fetches `assimp` (and its `contrib/draco` vendored submodule) from GitHub — this takes a few minutes on a cold cache.
- Subsequent builds use the cached `build/_deps/assimp-src/` directory and are fast.
- **No manual steps are required** to load Draco-compressed skin GLBs.

### Opting out (system Assimp)

If you have a system-installed Assimp that you want to use instead (e.g., because Draco is not needed), configure with:

```sh
cmake -DENGINE_ASSIMP_WITH_DRACO=OFF ..
```

Note: This will fail to load any Draco-compressed `.glb` at runtime unless the system Assimp was compiled with Draco support.

### CI / caching tip

Cache the `build/_deps/` directory between CI runs to avoid re-cloning Assimp on every build.

```yaml
# GitHub Actions example
- uses: actions/cache@v3
  with:
    path: build/_deps
    key: assimp-draco-${{ hashFiles('CMakeLists.txt') }}
```

### Option B — Disable Draco compression in AssetForge.py

If you cannot or do not want to build Assimp from source, export skins **without** Draco compression.  Edit `AssetForge.py` and remove (or set to `False`) the Draco export flag:

```python
# In AssetForge.py — disable Draco compression for standard Assimp compatibility
bpy.ops.export_scene.gltf(
    filepath=skin_path,
    export_draco_mesh_compression_enable=False,  # ← disable Draco
    ...
)
```

Then configure with `-DENGINE_ASSIMP_WITH_DRACO=OFF`.  Non-compressed GLBs load with any standard Assimp build.

---

## Testing the Modular Pipeline

1. **Place your assets:**
   ```
   src/Resources/skins/npc.glb
   src/Resources/animations/idle.glb
   src/Resources/animations/walk.glb
   ```

2. **Create a prefab** (`src/Resources/Prefabs/npc_modular.json`):
   ```json
   {
     "name": "npc_modular",
     "mesh": "skins/npc.glb",
     "animated": true,
     "components": {
       "AnimationController": {
         "default_state": "Idle",
         "animations": {
           "Idle": "animations/idle.glb",
           "Walk": "animations/walk.glb"
         }
       }
     }
   }
   ```

3. **Spawn in scene JSON or via EntityFactory:**
   ```json
   { "prefab": "npc_modular", "x": 10, "y": 0, "z": -20 }
   ```
   or:
   ```cpp
   EntityFactory::spawn(registry, "npc_modular", {10, 0, -20}, physics);
   ```

4. **Expected console output:**
   ```
   [AnimationLoader::loadSkin] Loaded 'src/Resources/skins/npc.glb': 1 mesh(es), 67 bone(s).
   [AnimationLoader::loadExternalAnimation] Loaded 'src/Resources/animations/idle.glb' (67/67 channels matched).
   [AnimationLoader::loadExternalAnimation] Loaded 'src/Resources/animations/walk.glb' (67/67 channels matched).
   ```

5. **If you see `0/N channels matched`:** Verify that the bone names in your skin and animation files match.  In Blender, both GLBs must be exported from the **same armature** with the **same bone names**.

---

## Backward Compatibility

All existing JSON prefabs and `SceneLoaderJson` `animated_characters` entries continue to work without modification.  The modular path is only activated when `components.AnimationController.animations` is a non-empty object.

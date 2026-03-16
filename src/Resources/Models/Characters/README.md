# Characters Models Directory

This directory holds animated GLB character assets.

## Current asset: `man-1.glb`

The primary character model is `man-1.glb`.  It contains five animation clips:

| Clip name in `.glb` | Mapped state name | Purpose |
|---|---|---|
| `Idle` | `Idle` | Standing still |
| `Casual_Walk` | `Walk` | Normal-pace locomotion |
| `Running` | `Run` | Fast locomotion |
| `Run_03` | `Sprint` | Full-sprint locomotion |
| `Walking` | `Patrol` | Slow patrol walk |

The mapping is configured in `src/Resources/scene.json` via `animation_map`.

## `animation_map` feature

Any `animated_characters` entry in `scene.json` (and prefab JSON files) may
include an optional `animation_map` object that bridges the engine's semantic
state names to the exact clip names baked into the `.glb`.

Use the `fallback_path` field to specify a second asset that the engine will
try **automatically** when the primary `path` fails to load (e.g. the primary
file exists on your local machine but is not committed to the repository).
When the fallback is used, `animation_map` is **not** applied — the fallback
asset uses `normalizeClipName()` auto-detection so the engine always has a
working mesh even on a fresh clone.

```json
"animated_characters": [
  {
    "path": "Characters/man-1.glb",
    "fallback_path": "Characters/characters.glb",
    "scale": 2.5,
    "x": 100.0,
    "y": "terrain",
    "z": -80.0,
    "animation_map": {
      "Idle":   "Idle",
      "Walk":   "Casual_Walk",
      "Run":    "Running",
      "Sprint": "Run_03",
      "Patrol": "Walking"
    }
  }
]
```

- **`path`** — preferred asset (loaded with `animation_map` when present)
- **`fallback_path`** — used automatically when `path` fails to load; `animation_map` is skipped and `normalizeClipName()` auto-detection is used instead
- **Left side (key)** — the state name registered in `AnimationController`.
  This is what engine systems, `setupDefaultTransitions()`, and Lua scripts use.
- **Right side (value)** — the exact clip name inside the `.glb` file
  (case-sensitive, as authored by the artist).

### Behaviour

| Condition | Behaviour |
|---|---|
| Primary `path` loads successfully + `animation_map` present | Only listed clips registered; `normalizeClipName()` NOT called. |
| Primary `path` loads successfully, no `animation_map` | `normalizeClipName()` auto-detection used. |
| Primary `path` fails + `fallback_path` present | Fallback asset loaded; `normalizeClipName()` auto-detection used regardless of `animation_map`. |
| Both paths fail | Error logged; entity not created. |

### Well-known state names (automatic transition wiring)

The engine's built-in locomotion system recognises four state names and wires
them automatically via `setupDefaultTransitions()`:

| State | Trigger |
|---|---|
| `Idle` | No movement keys held |
| `Walk` | Movement keys held (normal speed) |
| `Run` | Movement keys held (sprint / Shift) |
| `Jump` | Jump key pressed |

Any other state names (e.g. `Sprint`, `Patrol`, `Attack`, `Dance`) are
registered in `AnimationController` but have **no automatic transitions**.
They are available for programmatic use:

```cpp
// C++
controller->requestTransition("Patrol");
```

```lua
-- Lua AI script
engine.Animation.play("Patrol")
engine.Animation.play("Sprint")
engine.Animation.play("Idle")
```

## Prefab JSON support

Prefab files under `src/Resources/prefabs/` may also specify `animation_map`
at the top level or inside `components.AnimatedModelComponent`:

```json
{
  "mesh": "models/npc_guard.glb",
  "animated": true,
  "components": {
    "AnimatedModelComponent": {
      "animation_map": {
        "Idle":   "Guard_Idle",
        "Walk":   "Guard_Walk",
        "Attack": "Guard_Attack_01"
      }
    }
  }
}
```

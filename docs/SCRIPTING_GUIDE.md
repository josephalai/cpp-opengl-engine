# Engine Scripting & Interaction Guide

> **Audience**: Game designers and script developers who want to add gameplay  
> without touching C++.  
> **Scope**: NPC dialogue, resource gathering, action loops, AI pausing,  
> stateful interactions, and custom content.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [The WalkTo Infrastructure (Built-in)](#2-the-walkto-infrastructure-built-in)
3. [Making an Object Interactable](#3-making-an-object-interactable)
4. [Writing the Lua Script](#4-writing-the-lua-script)
5. [The State Machine Return Value](#5-the-state-machine-return-value)
6. [The Engine API Reference](#6-the-engine-api-reference)
7. [Stateful Interactions (Persistent Variables)](#7-stateful-interactions-persistent-variables)
8. [Action Interruption](#8-action-interruption)
9. [NPC AI Pausing](#9-npc-ai-pausing)
10. [Complete Script Examples](#10-complete-script-examples)
    - [Stateful Guard Dialogue](#101-stateful-guard-dialogue)
    - [Wanderer with Penalty Cooldown](#102-wanderer-with-penalty-cooldown)
    - [Looping Resource Gathering (Woodcutting)](#103-looping-resource-gathering-woodcutting)
11. [Adding a New Interactable Entity](#11-adding-a-new-interactable-entity)
12. [Adding a New NPC](#12-adding-a-new-npc)
13. [Prefab JSON Reference](#13-prefab-json-reference)
14. [Troubleshooting](#14-troubleshooting)

---

## 1. Architecture Overview

This engine uses a strict **separation of concerns**:

```
┌──────────────────────────────────────────────────────────┐
│                     CLIENT (OpenGL)                      │
│  Mouse Right-Click → EntityPicker → EntityClickedEvent   │
│      → NetworkSystem.sendActionRequest(targetNetworkId)  │
└──────────────────────────────────────────────────────────┘
                           │  ENet UDP
                           ▼
┌──────────────────────────────────────────────────────────┐
│                SERVER (C++ Headless)                     │
│                                                          │
│  Receive ActionRequestPacket                             │
│    → Validate InteractableComponent exists               │
│    → Emplace ActionStateComponent on player entity       │
│    → Run A* pathfinding → Assign PathfindingComponent    │
│                                                          │
│  Server Tick Loop (configurable Hz):                     │
│    PathfindingSystem.update(dt)  ← steers player         │
│    InteractionSystem.update(dt)  ← checks distance       │
│      if dist ≤ interact_range:                           │
│        remove PathfindingComponent (stop walking)        │
│        countdown actionTimer                             │
│        when timer == 0:                                  │
│          LuaScriptEngine.executeInteraction(...)         │
│                                                          │
│  ServerNPCManager.tick(dt)       ← drives NPC AI        │
│    respects pauseTimer per NPC                           │
└──────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────┐
│                  LUA SCRIPT                              │
│  on_interact(player_id, target_id, engine)               │
│    engine.Network.sendMessage(player_id, "Hello!")       │
│    engine.AI.pause(target_id, 5.0)                       │
│    engine.Stats.getLevel(player_id, "Woodcutting")       │
│    engine.Inventory.addItem(player_id, "Logs", 1)        │
│    return cooldown_seconds   ← 0.0 = stop, >0 = loop    │
└──────────────────────────────────────────────────────────┘
```

### Division of Responsibilities

| Layer | Responsibility |
|-------|----------------|
| **C++ Engine** | Physics, pathfinding, distance checking, networking, ENet packets, timer countdown, action interruption |
| **Lua Scripts** | Game logic: dialogue, rewards, animations, AI pausing, stat checks, RNG rolls |

> **Rule**: If it moves, collides, or travels over the network → C++.  
> If it has game meaning → Lua.

---

## 2. The WalkTo Infrastructure (Built-in)

**You never need to write WalkTo logic.** It is baked into the C++ engine.

When a player right-clicks an interactable object:

1. The client sends an `ActionRequestPacket` to the server.
2. The server validates that the target has an `InteractableComponent`.
3. C++ attaches an `ActionStateComponent` to the player entity.
4. C++ runs A\* pathfinding and attaches a `PathfindingComponent`.
5. `PathfindingSystem::update()` steers the player toward the target each tick.
6. `InteractionSystem::update()` polls distance. When `dist ≤ interact_range`:
   - `PathfindingComponent` is removed (player stops).
   - The Lua script's `on_interact()` is called.

### Cancellation

If the player presses **WASD** at any point during the approach or while an
action is active, `ServerMain` immediately strips both `ActionStateComponent`
and `PathfindingComponent`. The player regains manual control instantly and any
looping script (e.g. woodcutting) is silently terminated.

---

## 3. Making an Object Interactable

Add the `InteractableComponent` block to the entity's **prefab JSON**:

```json
{
  "id": "my_npc",
  "mesh": "models/walkrun_and_idle.glb",
  "animated": true,
  "components": {
    "InteractableComponent": {
      "script": "scripts/interactions/my_npc.lua",
      "interact_range": 2.5
    }
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `script` | string | Path relative to `src/Resources/`, e.g. `"scripts/interactions/guard.lua"` |
| `interact_range` | float | Distance in world-units at which the interaction fires. `2.0` is about arm's reach. |

---

## 4. Writing the Lua Script

Every interaction script **must** define the global function `on_interact`.

```lua
function on_interact(player_id, target_id, engine)
    -- player_id  : uint32  — the NetworkIdComponent::id of the acting player
    -- target_id  : uint32  — the NetworkIdComponent::id of the object clicked
    -- engine     : table   — the full C++ API bridge (see Section 6)

    engine.Network.sendMessage(player_id, "Hello!")

    return 0.0   -- see Section 5
end
```

### Isolated Environments

Each script path gets its **own isolated Sol2 environment**. Module-level
variables (outside functions) are private to that script and persist for the
lifetime of the server. Two different scripts can both define `on_interact`
without conflict.

```lua
-- This variable is private to THIS script file.
local visit_count = {}

function on_interact(player_id, target_id, engine)
    visit_count[player_id] = (visit_count[player_id] or 0) + 1
    engine.Network.sendMessage(player_id,
        "You have visited " .. visit_count[player_id] .. " times.")
    return 0.0
end
```

---

## 5. The State Machine Return Value

The `float` returned by `on_interact` controls the C++ Interaction State
Machine:

| Return value | Meaning |
|---|---|
| `return 0.0` | **Action complete.** C++ releases the player to IDLE immediately. |
| `return N` (N > 0) | **Loop.** C++ holds the player in ACTION state, waits exactly `N` seconds, then calls this script again. The loop continues until the script returns `0.0` or the player moves. |

```lua
-- Woodcutting example: keep chopping every 2.4 seconds
function on_interact(player_id, target_id, engine)
    engine.Network.sendMessage(player_id, "CHOP CHOP CHOP...")
    if engine.Math.rollChance(0.25) then
        engine.Network.sendMessage(player_id, "You got some logs!")
        return 0.0      -- stop the loop
    end
    return 2.4          -- try again after 2.4 seconds
end
```

---

## 6. The Engine API Reference

The `engine` table is passed to every `on_interact()` call. It contains the
following sub-tables:

---

### `engine.Network`

| Function | Signature | Description |
|----------|-----------|-------------|
| `sendMessage` | `(player_id: uint32, msg: string)` | Sends a reliable ENet `ServerMessagePacket` to the specific client. Appears as `[NPC DIALOGUE]: <msg>` in the client terminal. Will be routed to the ImGui chat box in a future phase. |
| `broadcastAnimation` | `(entity_id: uint32, anim: string)` | Tells all clients to play an animation on the entity (stub — logs to server console, will broadcast in a future phase). |
| `broadcastDamageSplat` | `(entity_id: uint32, dmg: int)` | Shows a damage number above the entity on all clients (stub). |
| `sendOpenUI` | `(player_id: uint32, ui_name: string)` | Sends a UI-open command to a specific client (stub). |

---

### `engine.Stats`

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `getLevel` | `(entity_id, skill_name: string)` | `int` | Returns the current level for the given skill name (stub returns 1). |
| `getAll` | `(entity_id)` | `table` | Returns a table of all skill levels. |

---

### `engine.Inventory`

| Function | Signature | Description |
|----------|-----------|-------------|
| `addItem` | `(player_id, item_name: string, count: int)` | Adds items to the player's inventory (stub — logs to server console). |
| `hasItem` | `(player_id, item_name: string)` → `bool` | Returns true if the player has at least one of the item (stub returns false). |

---

### `engine.Health`

| Function | Signature | Description |
|----------|-----------|-------------|
| `dealDamage` | `(entity_id, amount: int)` | Deals damage to an entity (stub). |
| `isDead` | `(entity_id)` → `bool` | Returns true if entity has 0 HP (stub returns false). |

---

### `engine.AI`

| Function | Signature | Description |
|----------|-----------|-------------|
| `pause` | `(npc_id: uint32, duration: float)` | **Freezes the NPC's AI** input generation for `duration` seconds. The NPC stands still on the server (zero-movement input). After the timer expires, normal wandering resumes. Use during dialogue so the NPC doesn't walk away. |

```lua
-- Stop the wanderer from walking away for 5 seconds
engine.AI.pause(target_id, 5.0)
```

---

### `engine.Math`

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `rollChance` | `(probability: float)` | `bool` | Pass a value between 0.0 and 1.0. Returns `true` with that probability. Uses a thread-local LCG seeded from `std::random_device`. |

```lua
if engine.Math.rollChance(0.25) then   -- 25% chance
    engine.Network.sendMessage(player_id, "Critical hit!")
end
```

---

### `engine.Entities`

| Function | Signature | Description |
|----------|-----------|-------------|
| `destroy` | `(entity_id: uint32)` | Marks the entity for removal from the world (stub — logs intent). |

---

### `engine.Transform`

| Function | Signature | Description |
|----------|-----------|-------------|
| `lookAt` | `(entityA_id, entityB_id)` | Rotates entity A to face entity B (stub). |

---

### `engine.CombatMath`

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `calculateMeleeHit` | `(attackerStats: table, defenderStats: table)` | `int` | Returns a damage value based on attacker and defender stats (stub returns 1–5). |

---

### `engine.Equipment`

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `getWeaponSpeed` | `(player_id)` | `float` | Returns the attack speed in seconds (stub returns 2.4). |

---

### `engine.Loot`

| Function | Signature | Description |
|----------|-----------|-------------|
| `generateDrop` | `(entity_id, loot_table: string)` | Rolls the loot table and drops items (stub). |

---

## 7. Stateful Interactions (Persistent Variables)

Module-level Lua variables persist for the **server's lifetime**. Use tables
keyed by `player_id` to store per-player state:

```lua
-- Module-level: persists between calls
local kills = {}

function on_interact(player_id, target_id, engine)
    kills[player_id] = (kills[player_id] or 0) + 1
    engine.Network.sendMessage(player_id,
        "Total kills this session: " .. kills[player_id])
    return 0.0
end
```

> **Note**: State is lost on server restart. For persistent progression
> (e.g. XP between sessions) you will need a database integration
> (planned for a future phase).

---

## 8. Action Interruption

The engine automatically cancels any ongoing interaction when the player
presses **WASD**:

- `ActionStateComponent` is removed → the Lua loop never fires again.
- `PathfindingComponent` is removed → auto-walk stops immediately.
- The player regains full manual control.

This is handled entirely in C++ (`ServerMain.cpp`, `PlayerInput` receive block).
**You do not need to write any cancellation logic in Lua.**

Practical impact:
- Woodcutting loop → player presses W → loop ends instantly.
- Walking to NPC → player presses S → walk aborts, NPC ignores the event.

---

## 9. NPC AI Pausing

Wandering NPCs are driven by `ServerNPCManager::tick()` which generates
synthetic `PlayerInputPacket` events each tick. During dialogue this makes the
NPC keep moving, which looks wrong.

Use `engine.AI.pause(target_id, seconds)` to freeze the NPC:

```lua
function on_interact(player_id, target_id, engine)
    -- Freeze for 5 seconds so the NPC faces us and stands still
    engine.AI.pause(target_id, 5.0)
    engine.Network.sendMessage(player_id, "NPC: Hello there!")
    return 0.0
end
```

Internally:
1. `engine.AI.pause(target_id, 5.0)` calls `onPauseNpc_(target_id, 5.0)`.
2. The C++ closure calls `npcManager.setPauseTimer(target_id, 5.0)`.
3. `ServerNPCManager::tick()` skips input generation for that NPC while `pauseTimer > 0`.
4. After 5 seconds the NPC resumes its normal wander pattern.

---

## 10. Complete Script Examples

### 10.1 Stateful Guard Dialogue

**File**: `src/Resources/scripts/interactions/guard.lua`

The guard delivers a unique line each time the player clicks, advancing
sequentially through a fixed script. State is per-player.

```lua
local dialogue_index = {}

local lines = {
    "Halt! Who goes there?",
    "I've got my eye on you, traveler.",
    "Move along, citizen.",
    "I used to be an adventurer like you, then I took an arrow in the knee.",
    "The guard stares at you blankly."
}

function on_interact(player_id, target_id, engine)
    local idx = dialogue_index[player_id] or 1
    engine.Network.sendMessage(player_id, "Guard: " .. (lines[idx] or lines[#lines]))
    if idx < #lines then
        dialogue_index[player_id] = idx + 1
    end
    return 0.0
end
```

---

### 10.2 Wanderer with Penalty Cooldown

**File**: `src/Resources/scripts/interactions/wanderer.lua`

The wanderer pauses their walk, talks up to 3 times, then imposes a 45-second
cooldown. Talking during the cooldown resets it.

```lua
local interaction_counts = {}
local penalty_start = {}
local MAX_FREE_INTERACTIONS = 3
local PENALTY_DURATION = 45
local NPC_PAUSE_DURATION = 5.0

function on_interact(player_id, target_id, engine)
    engine.AI.pause(target_id, NPC_PAUSE_DURATION)

    local now = os.time()

    -- Penalty check
    local pstart = penalty_start[player_id]
    if pstart ~= nil and (now - pstart) < PENALTY_DURATION then
        engine.Network.sendMessage(player_id, "Wanderer: Fuck off, peach fuzz.")
        penalty_start[player_id] = now   -- reset timer from this click
        return 0.0
    end

    -- Normal dialogue
    local count = interaction_counts[player_id] or 0
    if count == 0 then
        engine.Network.sendMessage(player_id, "Wanderer: Hi there! Lovely weather.")
    elseif count == 1 then
        engine.Network.sendMessage(player_id, "Wanderer: Again? Do you mind?")
    elseif count == 2 then
        engine.Network.sendMessage(player_id, "Wanderer: STOP BOTHERING ME.")
        penalty_start[player_id] = now
        interaction_counts[player_id] = 0
        return 0.0
    end

    interaction_counts[player_id] = count + 1
    return 0.0
end
```

---

### 10.3 Looping Resource Gathering (Woodcutting)

**File**: `src/Resources/scripts/skills/woodcutting.lua`

A repeating action. Returns `2.4` to loop every 2.4 seconds. Returns `0.0`
when the tree is felled or to let C++ stop on WASD.

```lua
local BASE_SUCCESS_RATE = 0.25
local LEVEL_BONUS       = 0.01
local DEPLETION_CHANCE  = 0.10
local SWING_COOLDOWN    = 2.4

function on_interact(player_id, target_id, engine)
    engine.Network.broadcastAnimation(player_id, "Chop_Axe")
    engine.Network.sendMessage(player_id, "CHOP CHOP CHOP...")

    local level = engine.Stats.getLevel(player_id, "Woodcutting")
    if engine.Math.rollChance(BASE_SUCCESS_RATE + level * LEVEL_BONUS) then
        engine.Inventory.addItem(player_id, "Logs", 1)
        engine.Network.sendMessage(player_id, "You successfully chopped the wood!")
        if engine.Math.rollChance(DEPLETION_CHANCE) then
            engine.Entities.destroy(target_id)
            return 0.0   -- tree is gone, stop loop
        end
    end

    return SWING_COOLDOWN   -- try again in 2.4 seconds
end
```

---

## 11. Adding a New Interactable Entity

**Step 1**: Create or edit the prefab JSON in `src/Resources/prefabs/`.

```json
{
  "id": "my_chest",
  "model_type": "my_chest",
  "mesh": "models/chest.glb",
  "animated": false,
  "physics": {
    "type": "static",
    "shape": "box",
    "halfExtents": [0.5, 0.5, 0.5]
  },
  "components": {
    "InteractableComponent": {
      "script": "scripts/interactions/chest.lua",
      "interact_range": 1.5
    }
  }
}
```

**Step 2**: Create the script `src/Resources/scripts/interactions/chest.lua`:

```lua
function on_interact(player_id, target_id, engine)
    engine.Network.sendMessage(player_id, "You open the chest. It is empty.")
    return 0.0
end
```

**Step 3**: Place the entity in the scene.

For static world objects: add it to `scene.json` → run `./asset_baker` to
bake it into `baked_chunks/`.

For dynamic NPCs: add a `{ "prefab": "my_chest", ... }` entry to
`src/Resources/npcs.json`.

---

## 12. Adding a New NPC

**Step 1**: Create `src/Resources/prefabs/npc_mycharacter.json`:

```json
{
  "id": "npc_mycharacter",
  "model_type": "npc_mycharacter",
  "mesh": "models/walkrun_and_idle.glb",
  "animated": true,
  "physics": {
    "type": "character_controller",
    "radius": 0.5,
    "height": 1.8
  },
  "components": {
    "AnimatedModelComponent": {
      "scale": 1.5,
      "model_rotation": { "x": -90.0, "y": 0.0, "z": 0.0 },
      "model_offset": { "x": 0.0, "y": -0.9, "z": 0.0 }
    },
    "AIComponent": {
      "script": "WanderAI"
    },
    "InteractableComponent": {
      "script": "scripts/interactions/mycharacter.lua",
      "interact_range": 2.0
    }
  }
}
```

**Step 2**: Add the AI script `src/Resources/scripts/ai/wander.lua` already
exists; use `WanderAI` or `GuardAI` as the `AIComponent.script` value.

**Step 3**: Add a spawn entry to `src/Resources/npcs.json`:

```json
{
  "npc_id": 10,
  "prefab": "npc_mycharacter",
  "model_type": "npc_mycharacter",
  "position": { "x": 120.0, "y": 3.0, "z": -60.0 },
  "script": "WanderAI"
}
```

**Step 4**: Create the interaction script.

---

## 13. Prefab JSON Reference

### Humanoid NPC AnimatedModelComponent (all fields)

```json
"AnimatedModelComponent": {
  "scale": 1.5,
  "model_rotation": { "x": -90.0, "y": 0.0, "z": 0.0 },
  "model_offset":   { "x": 0.0,   "y": -0.9, "z": 0.0 }
}
```

| Field | Purpose |
|-------|---------|
| `scale` | Uniform scale. `1.5` = close to real-world human height from the GLB base. |
| `model_rotation` | Corrects coordinate system. GLB exports are Y-up; the engine is Y-up but models need `-90° X` to stand upright. |
| `model_offset` | Vertical offset so the model's feet align with the physics capsule origin. |

### Physics types

| `type` | Usage |
|--------|-------|
| `"kinematic_character"` | Player (full physics, gravity, step-up) |
| `"character_controller"` | NPC (physics-driven, pathfinding-compatible) |
| `"static"` | Immovable world objects (trees, rocks, buildings) |

---

## 14. Troubleshooting

### "NPC keeps walking away during dialogue"
Call `engine.AI.pause(target_id, N)` at the start of `on_interact()`.  
`N` should be at least as long as the total interaction time (e.g. `5.0`).

### "Script fires but player sees nothing"
- Confirm `engine.Network.sendMessage(player_id, ...)` is called (not `target_id`).
- Check the server console for `[Lua] sendMessage(...)` output — if missing,
  the script returned before reaching that line.

### "Player can't reach the NPC"
- Increase `interact_range` in the prefab's `InteractableComponent`.
- Verify the target has a physics collider registered in `networkIdToEntity`.

### "Action never fires (player just stops near target)"
- Ensure the target entity has an `InteractableComponent` in its prefab.
- Check the server log for `ActionRequest denied — target X has no InteractableComponent`.

### "Script doesn't loop even though I return > 0"
- Make sure `on_interact` returns a **positive number**, not `nil` or `0.0`.
- The C++ `InteractionSystem` checks `cooldown > 0.0f` to decide whether to
  loop.

### "NPC doesn't speak (no dialogue message on client)"
- Run both server and client. The `ServerMessagePacket` travels over ENet —
  it will not appear if you are only running one side.
- Confirm `setSendMessageCallback` is wired in `ServerMain.cpp` (it is by
  default; check if the `interactionLua` object is the same instance as the one
  used by `InteractionSystem`).

### "Player can walk through the NPC"
- Ensure the NPC prefab has `"physics": { "type": "character_controller", ... }`.
- Physics bodies are only created server-side. Client-side visibility (collider
  for picking) uses `ColliderComponent` (set in `Engine.cpp::onNetworkSpawn`).

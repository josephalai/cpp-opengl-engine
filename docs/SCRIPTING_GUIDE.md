# Scripting Guide — Script-Driven Interaction State Machine

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [The Anatomy of a Click](#2-the-anatomy-of-a-click)
3. [Creating Your First Interaction Script](#3-creating-your-first-interaction-script)
4. [The Engine API Reference](#4-the-engine-api-reference)
5. [Return Value Conventions](#5-return-value-conventions)
6. [Advanced Patterns](#6-advanced-patterns)
7. [Hot Reloading](#7-hot-reloading)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Architecture Overview

The engine uses a **pure simulation layer** pattern where C++ owns memory, pathfinding, distance checking, and networking — while **Lua handles all game logic**.

```
┌─────────────────────────────────────────────────────┐
│  C++ Engine (server-side)                           │
│                                                     │
│  PathfindingSystem  →  InteractionSystem            │
│       (walk)               (distance check)         │
│                                    ↓                │
│                          LuaScriptEngine            │
│                          executeInteraction()        │
│                                    ↓                │
│                    ┌───────────────────────────┐    │
│                    │  woodcutting.lua          │    │
│                    │  on_interact(pid, tid, e) │    │
│                    │  → game logic here        │    │
│                    │  return 2.4  -- cooldown  │    │
│                    └───────────────────────────┘    │
└─────────────────────────────────────────────────────┘
```

### Responsibility Split

| Layer | Responsibility |
|-------|---------------|
| **C++ ECS** | Entity storage, component lifecycle, memory management |
| **C++ PathfindingSystem** | NavMesh A* pathfinding, auto-steering |
| **C++ InteractionSystem** | Distance checking, timer management, script invocation |
| **C++ LuaScriptEngine** | Lua VM, isolated script environments, API table injection |
| **Lua Scripts** | All game logic: RNG, item drops, damage formulas, dialogue |

The `InteractionSystem` is **completely blind** to game design. It knows only:
- Which entity the player wants to interact with.
- How far away the player is from the target.
- When to call the Lua script.

---

## 2. The Anatomy of a Click

Here is the complete lifecycle of a single right-click on a Tree entity:

```
1. USER RIGHT-CLICKS
       │
       ▼
2. EntityPicker (client)
   Ray-AABB intersection test
   → returns entt::entity of closest hit
       │
       ▼
3. NetworkSystem::sendActionRequest()
   ActionRequestPacket { targetNetworkId: 4052 }  ──── over ENet ──▶ SERVER
                                                                        │
       ┌────────────────────────────────────────────────────────────────┘
       ▼
4. Server receives ActionRequestPacket
   • Validates target entity 4052 exists
   • Validates target has InteractableComponent
   • Attaches ActionStateComponent to player
   • Issues PathfindingComponent → player starts moving
       │
       ▼  (every server tick)
5. PathfindingSystem::update(dt)
   → Player walks toward target
       │
       ▼
6. InteractionSystem::update(dt)
   • Checks distance: player vs target
   • If dist ≤ interactRange: arrived! removes PathfindingComponent
   • Decrements actionTimer
   • When timer ≤ 0: calls LuaScriptEngine::executeInteraction()
       │
       ▼
7. on_interact(player_id, target_id, engine)  ← Lua script executes
   • engine.Network.broadcastAnimation(...)
   • engine.Inventory.addItem(...)
   • engine.Entities.destroy(target_id)       ← optional: deplete resource
   return 2.4  ← cooldown seconds (0.0 = done)
       │
       ▼
8. InteractionSystem routes based on return value:
   • > 0.0  → set actionTimer = cooldown, loop
   • = 0.0  → remove ActionStateComponent, player is free
```

---

## 3. Creating Your First Interaction Script

### Step 1: Create the Prefab JSON

Create or edit a prefab in `src/Resources/prefabs/`. Add an `"InteractableComponent"` block:

```json
{
  "alias": "my_rock",
  "model_type": "my_rock",
  "physics": {
    "type": "static",
    "shape": "box",
    "halfExtents": [0.5, 0.5, 0.5]
  },
  "InteractableComponent": {
    "script": "scripts/skills/mining.lua",
    "interact_range": 1.5
  }
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `script` | `string` | Relative path from `src/Resources/` to the `.lua` file |
| `interact_range` | `float` | Distance in metres the player must be within to begin |

### Step 2: Write the Lua Script

Create `src/Resources/scripts/skills/mining.lua`:

```lua
-- scripts/skills/mining.lua
-- Resource Gathering: Mining

function on_interact(player_id, target_id, engine)
    engine.Network.broadcastAnimation(player_id, "Swing_Pickaxe")

    local mining_level = engine.Stats.getLevel(player_id, "Mining")

    if engine.Math.rollChance(0.15 + (mining_level * 0.005)) then
        engine.Inventory.addItem(player_id, "Iron Ore", 1)
        engine.Network.sendMessage(player_id, "You mine some iron ore.")

        if engine.Math.rollChance(0.08) then
            engine.Entities.destroy(target_id)
            return 0.0  -- Rock depleted
        end
    end

    return 3.0  -- 3-second mining cycle
end
```

### Step 3: Register the Prefab

If using the PrefabManager with a `prefabs.json` manifest, add your new prefab:

```json
{
  "prefabs": ["tree.json", "npc_goblin.json", "my_rock.json"]
}
```

Or place the file in the prefabs directory — PrefabManager loads all `.json` files automatically if configured to do so.

### Step 4: Place the Entity in the Scene

In `scene.json`, add your entity to the `"entities"` array:

```json
{
  "entities": [
    { "prefab": "my_rock", "position": [120.0, 0.0, -80.0] }
  ]
}
```

### Step 5: Test with the Server

Start the headless server:

```bash
cd build && ./headless_server
```

Connect with the client, right-click the rock, and observe the server logs:

```
[LuaScriptEngine] Loaded interaction script: scripts/skills/mining.lua
[Lua] broadcastAnimation(101, "Swing_Pickaxe")
[Lua] Stats.getLevel(skill="Mining") -> 1
[Lua] Math.rollChance(0.155) -> true
[Lua] Inventory.addItem(101, "Iron Ore", 1)
[Lua] sendMessage(101, "You mine some iron ore.")
[Lua] Math.rollChance(0.08) -> false
```

---

## 4. The Engine API Reference

The `engine` table is passed as the third argument to every `on_interact()` call. It provides the following sub-tables:

---

### `engine.Network`

Client-side communication from the server.

#### `engine.Network.broadcastAnimation(player_id, anim_name)`
Broadcasts an animation trigger to all nearby clients.

| Parameter | Type | Description |
|-----------|------|-------------|
| `player_id` | `uint32` | Network ID of the entity to animate |
| `anim_name` | `string` | Animation clip name (e.g. `"Chop_Axe"`, `"Slash_Sword"`) |

**Example:**
```lua
engine.Network.broadcastAnimation(player_id, "Chop_Axe")
```

---

#### `engine.Network.sendMessage(player_id, message)`
Sends a chat/notification message to a specific player's client.

| Parameter | Type | Description |
|-----------|------|-------------|
| `player_id` | `uint32` | Network ID of the receiving player |
| `message` | `string` | Message text |

**Example:**
```lua
engine.Network.sendMessage(player_id, "You get some logs.")
```

---

#### `engine.Network.sendOpenUI(player_id, ui_name)`
Sends a packet telling the client to open a specific UI panel.

| Parameter | Type | Description |
|-----------|------|-------------|
| `player_id` | `uint32` | Network ID of the target player |
| `ui_name` | `string` | UI identifier (e.g. `"UI_BANKER_DIALOGUE"`, `"UI_SHOP"`) |

**Example:**
```lua
engine.Network.sendOpenUI(player_id, "UI_BANKER_DIALOGUE")
```

---

#### `engine.Network.broadcastDamageSplat(target_id, damage)`
Broadcasts a floating damage number to nearby clients.

| Parameter | Type | Description |
|-----------|------|-------------|
| `target_id` | `uint32` | Network ID of the damaged entity |
| `damage` | `integer` | Damage value to display |

**Example:**
```lua
engine.Network.broadcastDamageSplat(target_id, 5)
```

---

### `engine.Stats`

Access entity skill levels and statistics.

#### `engine.Stats.getLevel(entity_id, skill_name) → integer`
Returns the skill level for the given entity.

| Parameter | Type | Description |
|-----------|------|-------------|
| `entity_id` | `uint32` | Network ID of the entity |
| `skill_name` | `string` | Skill name (e.g. `"Woodcutting"`, `"Mining"`, `"Attack"`) |

**Returns:** `integer` — skill level (default stub returns `1`)

**Example:**
```lua
local wc_level = engine.Stats.getLevel(player_id, "Woodcutting")
```

---

#### `engine.Stats.getAll(entity_id) → table`
Returns a table of all stat levels for the entity.

| Parameter | Type | Description |
|-----------|------|-------------|
| `entity_id` | `uint32` | Network ID of the entity |

**Returns:** `table` — `{ attack=N, defence=N, strength=N, ... }`

**Example:**
```lua
local stats = engine.Stats.getAll(player_id)
local atk = stats.attack
```

---

### `engine.Inventory`

Manage player inventory items.

#### `engine.Inventory.addItem(player_id, item_name, count)`
Adds items to a player's inventory.

| Parameter | Type | Description |
|-----------|------|-------------|
| `player_id` | `uint32` | Network ID of the receiving player |
| `item_name` | `string` | Item identifier (e.g. `"Logs"`, `"Iron Ore"`) |
| `count` | `integer` | Number of items to add |

**Example:**
```lua
engine.Inventory.addItem(player_id, "Logs", 1)
```

---

### `engine.Health`

Query and modify entity health.

#### `engine.Health.dealDamage(target_id, amount)`
Applies damage to an entity's health.

| Parameter | Type | Description |
|-----------|------|-------------|
| `target_id` | `uint32` | Network ID of the damaged entity |
| `amount` | `integer` | Amount of damage to deal |

**Example:**
```lua
engine.Health.dealDamage(target_id, 5)
```

---

#### `engine.Health.isDead(target_id) → boolean`
Returns `true` if the entity's health has reached zero.

**Example:**
```lua
if engine.Health.isDead(target_id) then
    engine.Entities.destroy(target_id)
    return 0.0
end
```

---

### `engine.Entities`

Manage entity lifecycle.

#### `engine.Entities.destroy(target_id)`
Destroys the target entity from the ECS registry. After this call, the entity no longer exists. The interaction state machine automatically detects this and cleans up the `ActionStateComponent`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `target_id` | `uint32` | Network ID of the entity to destroy |

**Example:**
```lua
engine.Entities.destroy(target_id)
return 0.0  -- Must return 0 after destroying — no target to continue with
```

---

### `engine.Math`

Deterministic mathematics utilities.

#### `engine.Math.rollChance(probability) → boolean`
Rolls a random check. Returns `true` with the given probability.

| Parameter | Type | Description |
|-----------|------|-------------|
| `probability` | `number` | Probability in range `[0.0, 1.0]` |

**Returns:** `boolean` — `true` if the roll succeeds

**Example:**
```lua
if engine.Math.rollChance(0.10) then
    -- 10% chance
end
```

---

### `engine.Transform`

Entity transform manipulation.

#### `engine.Transform.lookAt(entity_id, target_id)`
Rotates an entity to face the target entity.

| Parameter | Type | Description |
|-----------|------|-------------|
| `entity_id` | `uint32` | Network ID of the entity to rotate |
| `target_id` | `uint32` | Network ID of the entity to face |

**Example:**
```lua
engine.Transform.lookAt(player_id, target_id)
```

---

### `engine.CombatMath`

Combat formula calculations.

#### `engine.CombatMath.calculateMeleeHit(attacker_stats, defender_stats) → integer`
Calculates melee damage using attacker and defender stat tables.

| Parameter | Type | Description |
|-----------|------|-------------|
| `attacker_stats` | `table` | Stats table (from `engine.Stats.getAll`) |
| `defender_stats` | `table` | Stats table (from `engine.Stats.getAll`) |

**Returns:** `integer` — damage value

**Example:**
```lua
local damage = engine.CombatMath.calculateMeleeHit(
    engine.Stats.getAll(player_id),
    engine.Stats.getAll(target_id)
)
```

---

### `engine.Equipment`

Equipment query utilities.

#### `engine.Equipment.getWeaponSpeed(player_id) → number`
Returns the attack speed of the player's currently equipped weapon in seconds.

| Parameter | Type | Description |
|-----------|------|-------------|
| `player_id` | `uint32` | Network ID of the player |

**Returns:** `number` — weapon attack cooldown in seconds (stub default: `2.4`)

**Example:**
```lua
local speed = engine.Equipment.getWeaponSpeed(player_id)
return speed
```

---

### `engine.Loot`

Loot table generation.

#### `engine.Loot.generateDrop(target_id, loot_table)`
Generates item drops from the named loot table and spawns them near the target's position.

| Parameter | Type | Description |
|-----------|------|-------------|
| `target_id` | `uint32` | Network ID of the entity that died |
| `loot_table` | `string` | Loot table identifier (e.g. `"goblin_drop_table"`) |

**Example:**
```lua
engine.Loot.generateDrop(target_id, "goblin_drop_table")
```

---

## 5. Return Value Conventions

The return value of `on_interact()` is the **cooldown time in seconds** until the next script invocation:

| Return Value | Meaning | C++ Behaviour |
|-------------|---------|---------------|
| `> 0.0` | Action repeats after `cooldown` seconds | `actionTimer = cooldown`; loop continues |
| `= 0.0` | Action is complete | `ActionStateComponent` removed; player is free |
| No return | Treated as `0.0` | Action is complete |

### Common Patterns

```lua
-- Repeating action (e.g., woodcutting, mining, fishing)
return 2.4  -- Repeat every 2.4 seconds

-- Instant action (e.g., dialogue, gate opening)
return 0.0

-- Conditional end (e.g., resource depleted)
if depleted then
    engine.Entities.destroy(target_id)
    return 0.0
end
return 2.4  -- Still chopping
```

---

## 6. Advanced Patterns

### Multi-Step Interactions

Use the `phase` field of a state table (stored outside the function) to implement multi-step sequences. Since each Lua script runs in an **isolated environment**, you can use module-level variables:

```lua
-- scripts/interactions/quest_npc.lua
local step = 0

function on_interact(player_id, target_id, engine)
    if step == 0 then
        engine.Network.sendOpenUI(player_id, "UI_QUEST_INTRO")
        step = 1
        return 3.0  -- Wait 3 seconds before next trigger
    elseif step == 1 then
        engine.Network.sendOpenUI(player_id, "UI_QUEST_ACCEPT")
        step = 0    -- Reset for next player
        return 0.0
    end
    return 0.0
end
```

### Conditional Branching

```lua
function on_interact(player_id, target_id, engine)
    local level = engine.Stats.getLevel(player_id, "Woodcutting")

    if level < 15 then
        engine.Network.sendMessage(player_id,
            "You need level 15 Woodcutting to chop this tree.")
        return 0.0  -- Immediately cancel
    end

    -- ... normal woodcutting logic ...
    return 2.4
end
```

### Resource Depletion with Respawn

```lua
function on_interact(player_id, target_id, engine)
    engine.Network.broadcastAnimation(player_id, "Chop_Axe")

    if engine.Math.rollChance(0.05) then
        engine.Inventory.addItem(player_id, "Magic Logs", 1)
        engine.Network.sendMessage(player_id, "You get some magic logs.")

        if engine.Math.rollChance(0.03) then
            -- Deplete: destroy the tree. A future spawn system will
            -- respawn a stump entity after a timer.
            engine.Entities.destroy(target_id)
            return 0.0
        end
    end

    return 4.0  -- Magic trees are slower: 4-second cycle
end
```

### Combat with Flee Mechanics

```lua
function on_interact(player_id, target_id, engine)
    engine.Network.broadcastAnimation(player_id, "Slash_Sword")

    local dmg = engine.CombatMath.calculateMeleeHit(
        engine.Stats.getAll(player_id),
        engine.Stats.getAll(target_id))

    engine.Health.dealDamage(target_id, dmg)
    engine.Network.broadcastDamageSplat(target_id, dmg)

    if engine.Health.isDead(target_id) then
        engine.Loot.generateDrop(target_id, "rat_drops")
        engine.Entities.destroy(target_id)
        engine.Network.sendMessage(player_id, "You have defeated the rat.")
        return 0.0
    end

    -- Chance to flee at low health (future: check HP ratio)
    if engine.Math.rollChance(0.05) then
        engine.Network.sendMessage(player_id, "The enemy tries to flee!")
        return 0.0  -- Combat ends; pathfinding toward new position handled elsewhere
    end

    return engine.Equipment.getWeaponSpeed(player_id)
end
```

---

## 7. Hot Reloading

Scripts are loaded lazily on first invocation and cached per `scriptPath` in `LuaScriptEngine::interactionEnvs_`. To hot-reload a script during a live server session:

1. **Clear the cache entry** (future API):
   ```cpp
   luaEngine.reloadScript("scripts/skills/woodcutting.lua");
   ```

2. **Restart the server** (simplest approach for development):
   ```bash
   # In the build directory:
   ./headless_server
   ```

Since scripts are isolated Sol2 environments, a reload only affects that script — other scripts continue running uninterrupted.

### Future Hot-Reload Implementation

The cache is stored in `LuaScriptEngine::interactionEnvs_`. To implement live reloading, erase the entry:

```cpp
// In LuaScriptEngine.cpp:
void LuaScriptEngine::reloadScript(const std::string& scriptPath) {
    interactionEnvs_.erase(scriptPath);
    std::cout << "[LuaScriptEngine] Script cache cleared: " << scriptPath << "\n";
    // Next call to executeInteraction() will reload from disk.
}
```

---

## 8. Troubleshooting

### `on_interact not defined in scripts/...`

**Cause:** The Lua file was found and loaded, but it doesn't define `on_interact`.

**Fix:** Ensure your script has the exact function signature:
```lua
function on_interact(player_id, target_id, engine)
    -- ...
end
```

---

### `Interaction script not found: .../src/Resources/scripts/...`

**Cause:** The `script` path in the prefab JSON is wrong, or the file doesn't exist.

**Fix:** Check the path is relative to `src/Resources/`. Example:
```json
"script": "scripts/skills/woodcutting.lua"
```
resolves to:
```
<project_root>/src/Resources/scripts/skills/woodcutting.lua
```

---

### Player walks to target but nothing happens

**Cause 1:** The target entity does not have an `InteractableComponent`. Verify the prefab JSON has an `"InteractableComponent"` block.

**Cause 2:** The target entity was not spawned via `EntityFactory::spawn()`. Only entities created through the factory get the component.

**Cause 3:** The `interact_range` is too small. Try increasing it temporarily to verify the system works:
```json
"interact_range": 10.0
```

---

### ActionState not assigned (server log: no "ActionState assigned" message)

**Cause:** The `ActionRequestPacket` is being received, but the target entity doesn't have an `InteractableComponent`. The server only assigns `ActionStateComponent` when the target is interactable.

**Fix:** Verify the prefab JSON is loaded and parsed correctly. Check server startup logs for any prefab loading errors from PrefabManager.

---

### Lua error: `attempt to index a nil value (field 'Network')`

**Cause:** The `engine` table was not passed correctly. This should not happen under normal usage — it indicates a bug in `LuaScriptEngine::executeInteraction()`.

**Fix:** Ensure you are calling `on_interact` with three arguments:
```lua
function on_interact(player_id, target_id, engine)
--                                         ^^^^^^ engine must be the 3rd arg
```

---

### Script runs once but then stops

**Cause:** `on_interact` returned `0.0` (or no value). The state machine ended the interaction.

**Fix:** Return a positive cooldown to keep the loop running:
```lua
return 2.4  -- Keep going
```

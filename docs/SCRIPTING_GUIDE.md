# Scripting Guide: Script-Driven Interaction State Machine

## Overview

This guide covers the **Script-Driven Interaction State Machine** (Step 6.1) — the architecture that transforms the C++ engine into a pure simulation layer. C++ handles memory, pathfinding, distance checking, and networking. **Lua handles all game logic.** Zero hardcoded C++ game logic — every interaction (woodcutting, combat, dialogue) is defined entirely in hot-reloadable Lua scripts.

This is the exact architecture used by industry giants (RuneScape, WoW private servers, MUDs) to build infinitely scalable MMOs.

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                     CLIENT                              │
│  Mouse Click → EntityPicker → EntityClickedEvent        │
│      → NetworkSystem.sendActionRequest(targetId)         │
└─────────────────────────────────────────────────────────┘
                           │  ENet UDP
                           ▼
┌─────────────────────────────────────────────────────────┐
│                     SERVER (C++)                        │
│                                                         │
│  Receive ActionRequestPacket                            │
│    → Validate InteractableComponent exists              │
│    → Emplace ActionStateComponent on player             │
│    → Run A* → Assign PathfindingComponent               │
│                                                         │
│  Server Tick Loop (10 Hz):                              │
│    PathfindingSystem.update(dt)  ← steers player        │
│    InteractionSystem.update(dt)  ← checks distance      │
│      if within range:                                   │
│        remove PathfindingComponent                      │
│        tick actionTimer down                            │
│        when timer == 0:                                 │
│          LuaScriptEngine.executeInteraction(...)        │
│            ↓                                            │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                  LUA SCRIPT                             │
│  on_interact(player_id, target_id, engine)              │
│    engine.Network.broadcastAnimation(...)               │
│    engine.Stats.getLevel(...)                           │
│    engine.Inventory.addItem(...)                        │
│    return cooldown_seconds  ← or 0.0 to stop            │
└─────────────────────────────────────────────────────────┘
```

### Division of Responsibilities

| Layer | Responsibility |
|-------|---------------|
| **C++ Engine** | Memory management, physics, pathfinding, distance checking, networking, timer countdown |
| **Lua Scripts** | All game logic: what happens when you interact, what reward you get, what animation plays |

---

## 2. The Anatomy of a Click

A single right-click triggers this complete lifecycle:

1. **The Intent (Client):** User right-clicks. Engine calculates a 3D ray from camera through mouse cursor.
2. **The Intersect (Client):** `EntityPicker` performs Ray-AABB intersection tests against all renderable entities with bounding boxes. Finds the closest hit entity.
3. **The Dispatch (Client):** If the hit entity has a `NetworkIdComponent`, the client sends `ActionRequestPacket { targetNetworkId: 4052 }` to the server. The client does **not** start any action — it waits.
4. **The Acknowledgment (Server C++):** Server receives packet. Validates Entity `4052` exists and has an `InteractableComponent` (which contains the Lua script path and required distance).
5. **The State Change (Server C++):** Server attaches `ActionStateComponent` to the player. Player's state is now in the interaction pipeline.
6. **The Chase (Server C++):** Server runs A* pathfinding and assigns `PathfindingComponent`. `PathfindingSystem` steers the player tick-by-tick.
7. **The Arrival (Server C++):** `InteractionSystem` detects player is within `interactRange` (e.g., 1.5 metres). Halts movement (removes `PathfindingComponent`) and flags `isArrived = true`.
8. **The Loop (Server C++):** Every server tick, `InteractionSystem` decrements `actionTimer`. When it hits 0, C++ calls the Lua script.
9. **The Logic & Reward (Lua → Client):** Lua executes, rolls RNG, tells C++ to add an item, broadcasts an animation packet, and returns a cooldown float. C++ routes the state machine accordingly.

---

## 3. Creating Your First Interaction Script

### Step 1: Create a Prefab JSON with `InteractableComponent`

Create or update a prefab in `src/Resources/prefabs/`:

```json
{
  "alias": "my_rock",
  "model_type": "my_rock",
  "physics": { "type": "static", "shape": "box", "halfExtents": [1.0, 1.0, 1.0] },
  "InteractableComponent": {
    "script": "scripts/skills/mining.lua",
    "interact_range": 1.5
  }
}
```

The `"InteractableComponent"` block can be at the **top level** or nested inside a `"components"` block — the EntityFactory handles both.

### Step 2: Write a Lua Script with `on_interact()`

Create `src/Resources/scripts/skills/mining.lua`:

```lua
function on_interact(player_id, target_id, engine)
    -- Play the mining animation
    engine.Network.broadcastAnimation(player_id, "Swing_Pickaxe")

    -- Get the player's Mining skill level
    local mining_level = engine.Stats.getLevel(player_id, "Mining")

    -- Roll success chance based on level
    if engine.Math.rollChance(0.25 + (mining_level * 0.01)) then
        engine.Inventory.addItem(player_id, "Copper Ore", 1)
        engine.Network.sendMessage(player_id, "You mine some copper ore.")
    end

    -- Return cooldown: 2.4 seconds (OSRS 4-tick cycle)
    return 2.4
end
```

### Step 3: Register the Prefab in Scene

The prefab is automatically available once the file exists in `src/Resources/prefabs/`. Reference it in `scene.json` or spawn it via the Asset Baker:

```json
{
  "entities": [
    { "alias": "my_rock", "x": 100.0, "y": 0.0, "z": -80.0 }
  ]
}
```

### Step 4: Test with the Server

Start the headless server:
```bash
./build/headless_server
```

You should see log output when the interaction fires:
```
[Lua] broadcastAnimation(101, "Swing_Pickaxe")
[Lua] Stats.getLevel(_, "Mining") -> 1
[Lua] Inventory.addItem(101, "Copper Ore", 1)
[Lua] sendMessage(101, "You mine some copper ore.")
```

---

## 4. The Engine API Reference

Every `on_interact` function receives an `engine` table as its third argument. This table exposes the following C++ subsystem APIs:

### `engine.Network`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Network.broadcastAnimation(playerId, animName)` | Broadcasts an animation event to nearby clients | void |
| `engine.Network.sendMessage(playerId, message)` | Sends a chat message to the player | void |
| `engine.Network.sendOpenUI(playerId, uiName)` | Tells the client to open a specific UI panel | void |
| `engine.Network.broadcastDamageSplat(targetId, damage)` | Broadcasts a floating damage number to nearby clients | void |

### `engine.Stats`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Stats.getLevel(entityId, skillName)` | Returns the entity's level in the given skill | integer |
| `engine.Stats.getAll(entityId)` | Returns a table of all skill levels for the entity | table |

### `engine.Inventory`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Inventory.addItem(playerId, itemName, count)` | Adds items to the player's inventory | void |
| `engine.Inventory.hasItem(playerId, itemName)` | Returns true if the player has the item | boolean |

### `engine.Health`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Health.dealDamage(targetId, amount)` | Applies damage to the target entity | void |
| `engine.Health.isDead(targetId)` | Returns true if the entity's health is zero or below | boolean |

### `engine.Entities`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Entities.destroy(targetId)` | Destroys the target entity from the ECS registry | void |

### `engine.Math`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Math.rollChance(probability)` | Rolls a deterministic RNG. Returns true with the given probability (0.0–1.0) | boolean |

### `engine.Transform`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Transform.lookAt(entityA, entityB)` | Rotates entity A to face entity B | void |

### `engine.CombatMath`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.CombatMath.calculateMeleeHit(attackerStats, defenderStats)` | Runs the combat formula and returns damage dealt | integer |

### `engine.Equipment`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Equipment.getWeaponSpeed(playerId)` | Returns the player's equipped weapon attack speed in seconds | float |

### `engine.Loot`

| Function | Description | Returns |
|----------|-------------|---------|
| `engine.Loot.generateDrop(targetId, lootTable)` | Generates a loot drop from the named loot table | void |

---

## 5. Return Value Conventions

The return value of `on_interact()` is a `float` that controls what the C++ state machine does next:

| Return Value | Meaning |
|-------------|---------|
| `> 0.0` | **Loop the action.** The `ActionStateComponent.actionTimer` is reset to this value (in seconds). The script will be called again after this delay. Use this for repeated actions like woodcutting or combat. |
| `0.0` | **End the action.** The `ActionStateComponent` is removed from the player. Use this for instant actions like dialogue, or when a resource is depleted or an enemy is killed. |

Examples:
```lua
return 2.4    -- Call me again in 2.4 seconds (OSRS 4-tick woodcutting cycle)
return 1.8    -- Fast attack speed
return 0.0    -- Action complete, clean up immediately
```

---

## 6. Advanced Patterns

### Multi-Step Interactions

Use a flag in the inventory or a custom tracking mechanism to implement multi-step quests. The Lua script can check game state and return different cooldowns:

```lua
function on_interact(player_id, target_id, engine)
    local has_knife = engine.Inventory.hasItem(player_id, "Knife")
    if not has_knife then
        engine.Network.sendMessage(player_id, "You need a knife to do that.")
        return 0.0  -- End immediately
    end
    -- ... rest of logic
    return 3.0
end
```

### Conditional Branching

Scripts can inspect entity state and behave differently:

```lua
function on_interact(player_id, target_id, engine)
    if engine.Health.isDead(target_id) then
        engine.Network.sendMessage(player_id, "It's already dead.")
        return 0.0
    end
    -- normal combat logic...
end
```

### Resource Depletion

Return `0.0` after destroying the target to stop the loop:

```lua
if engine.Math.rollChance(0.10) then
    engine.Network.sendMessage(player_id, "The tree falls.")
    engine.Entities.destroy(target_id)
    return 0.0  -- Stop — the tree is gone
end
return 2.4  -- Keep chopping
```

---

## 7. Hot Reloading

Each interaction script is loaded **once** and cached in `LuaScriptEngine::interactionEnvs_` by script path. To hot-reload a script during development:

1. **Restart the server** — the simplest approach for development builds.
2. **Clear the cache** — a future extension could expose a `/reload` console command that calls `interactionEnvs_.clear()`, forcing all scripts to be re-loaded on next invocation.

The scripts themselves have no compiled binary dependency — they are plain text files. Changing a `.lua` file and reloading the server takes effect immediately.

---

## 8. Troubleshooting

### "Interaction script not found"
```
[LuaScriptEngine] Interaction script not found: /path/to/src/Resources/scripts/skills/mining.lua
```
**Fix:** Check that the path in your prefab JSON matches the actual file location under `src/Resources/`.

### "on_interact not defined"
```
[LuaScriptEngine] on_interact not defined in: scripts/skills/mining.lua
```
**Fix:** Make sure your script defines the function as `function on_interact(player_id, target_id, engine)`.

### "ActionRequest denied — target has no InteractableComponent"
```
[Server] ActionRequest denied — target 4052 has no InteractableComponent.
```
**Fix:** Verify the prefab JSON for the target entity has an `"InteractableComponent"` block with a `"script"` field.

### "ActionRequest denied — target too far"
```
[Server] ActionRequest denied — target too far (cell dist 3).
```
**Fix:** The player is more than one SpatialGrid cell away from the target when the click is sent. Either: (a) the player needs to be closer before clicking, or (b) increase the SpatialGrid cell size in world_config.json.

### Script error at runtime
```
[LuaScriptEngine] Error in on_interact (scripts/skills/mining.lua): attempt to call nil value
```
**Fix:** You're calling an `engine.*` function that doesn't exist. Check the API reference table above for the correct function names.

---

## 9. Adding New API Functions

To add a new function to the `engine` table:

1. Open `src/Scripting/LuaScriptEngine.cpp`
2. Find `LuaScriptEngine::buildEngineTable()`
3. Add your function to the appropriate sub-table (or create a new one):

```cpp
// In buildEngineTable():
sol::table quest = lua_.create_table();
quest["completeStep"] = [](uint32_t playerId, const std::string& questId, int step) {
    std::cout << "[Lua] Quest.completeStep(" << playerId << ", \""
              << questId << "\", " << step << ")\n";
    // TODO: real implementation
};
engine["Quest"] = quest;
```

Then use it in Lua:
```lua
engine.Quest.completeStep(player_id, "Tutorial", 1)
```

---

## 10. Example Scripts

Three complete example scripts are included in `src/Resources/scripts/`:

| Script | Path | Use Case |
|--------|------|----------|
| Woodcutting | `scripts/skills/woodcutting.lua` | Resource gathering with RNG and depletion |
| Melee Combat | `scripts/combat/melee_combat.lua` | Combat with damage, death, and loot |
| Banker Dialogue | `scripts/interactions/banker_dialogue.lua` | Instant NPC dialogue that opens a UI |

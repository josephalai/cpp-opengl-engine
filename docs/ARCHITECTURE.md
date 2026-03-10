# Engine Architecture — Technical Reference

> **Engine Type:** Hybrid MMORPG (Star Wars Galaxies–style movement & sandbox × Old School RuneScape–style skilling & interaction)
>
> **Language:** C++17 · **ECS:** EnTT · **Physics:** Bullet 3 · **Networking:** ENet · **Scripting:** Lua 5.4 / Sol2 · **Rendering:** OpenGL 4.1 / GLFW / Assimp
>
> **Design principle — "Dumb Engine, Smart Data":** The C++ runtime is a neutral execution layer.  All game-specific behaviour — entity archetypes, physics tuning, AI logic, camera feel, and input bindings — is defined in JSON and Lua.  Recompilation is required only to change the engine itself, never to change the game.

---

## Table of Contents

1. [Architectural Philosophy](#1-architectural-philosophy)
2. [Server Implementation & Simulation](#2-server-implementation--simulation)
3. [The Scripting Engine (Lua / Sol2)](#3-the-scripting-engine-lua--sol2)
4. [Client-Server Interaction (The Pipeline)](#4-client-server-interaction-the-pipeline)
5. [Data Structure Examples](#5-data-structure-examples)

---

## 1. Architectural Philosophy

### 1.1 The GEA Build Wall

The project follows a **Game Engine Architecture (GEA) Build Wall** pattern, enforced at the CMake level.  Source files are split into three disjoint sets:

| Set | Description | Depends on |
|---|---|---|
| **COMMON** | Simulation core — ECS components, physics, networking, config, scripting, math utilities | EnTT, Bullet, GLM, nlohmann/json, Sol2/Lua |
| **SERVER** | Headless entry point (`ServerMain.cpp`), NPC management | COMMON |
| **CLIENT** | Full rendering pipeline — OpenGL, GLFW, Assimp, Freetype, Quill logging, all GUI/shader/terrain code | COMMON + OpenGL + GLFW + Assimp + … |

The wall is implemented in `CMakeLists.txt` with explicit glob rules:

```cmake
# ── COMMON: simulation-safe sources (no OpenGL, no GLFW) ──────────────
file(GLOB_RECURSE COMMON_SOURCES
    "${CMAKE_SOURCE_DIR}/src/ECS/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Physics/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Toolbox/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/World/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Network/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Util/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Collision/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Config/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Scripting/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Terrain/HeightMap.cpp"
)

# Any file in COMMON that accidentally touches OpenGL is explicitly excluded:
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/PhysicsDebugDrawer\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/CharacterController\\.cpp$")
# ... (additional excludes for Picker, MousePicker, TerrainPicker, MockServer)

# ── SERVER: headless executable ────────────────────────────────────────
file(GLOB_RECURSE SERVER_SOURCES "${CMAKE_SOURCE_DIR}/src/Server/*.cpp")

add_executable(headless_server ${COMMON_SOURCES} ${SERVER_SOURCES})

# ── CLIENT: full rendering executable ──────────────────────────────────
file(GLOB_RECURSE ALL_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h")
set(CLIENT_SOURCES ${ALL_SOURCES})
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*/Server/.*")

add_executable(client_engine ${CLIENT_SOURCES})
```

**Result:** Two independent executables are built from a single source tree.  Any source file in `COMMON` that mistakenly `#include`s an OpenGL header will fail to compile under `headless_server`, catching the violation at build time.

### 1.2 The `HEADLESS_SERVER` Preprocessor Guard

The headless server is given a compile definition that allows *code-level* branching where a source file legitimately needs to exist in both targets:

```cmake
target_compile_definitions(headless_server PRIVATE
    RESOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    HEADLESS_SERVER
)
```

Inside `EntityFactory.cpp`, for example, the mesh-loading path is compiled out for the server:

```cpp
#ifndef HEADLESS_SERVER
#include "../ECS/Components/AssimpModelComponent.h"
#endif

// ... inside EntityFactory::spawn():
#ifndef HEADLESS_SERVER
    if (prefab.contains("mesh")) {
        auto& amc = registry.emplace<AssimpModelComponent>(entity);
        amc.meshPath = prefab["mesh"].get<std::string>();
        // amc.mesh remains nullptr until the client's asset loader resolves it.
    }
#endif
```

This means `EntityFactory::spawn()` produces identical *simulation* components (Transform, NetworkId, InputState, InputQueue, Physics) on both server and client, while only the client additionally attaches the GPU-facing `AssimpModelComponent`.  The server binary has zero dependency on OpenGL, GLFW, Assimp, Freetype, PNG, or any rendering library.

---

## 2. Server Implementation & Simulation

### 2.1 Initialization — ConfigManager

Both executables call `ConfigManager::get().loadAll(HOME_PATH)` as the very first operation in their `main()` / `Engine::init()`.  This singleton reads three JSON files from `src/Resources/`:

| File | Parsed Into | Used By |
|---|---|---|
| `world_config.json` | `PhysicsConfig`, `ServerConfig` | Server + Client |
| `client_settings.json` | `ClientConfig` (window, FOV, camera) | Client only |
| `environment_presets.json` | `EnvironmentConfig` (sky, fog, lighting) | Client only |

```cpp
// src/Config/ConfigManager.cpp
void ConfigManager::loadAll(const std::string& resourceRoot) {
    const std::string base = resourceRoot + "/src/Resources/";
    loadWorldConfig(base + "world_config.json");
    loadClientSettings(base + "client_settings.json");
    loadEnvironmentPresets(base + "environment_presets.json");
    loaded_ = true;
}
```

The `loadWorldConfig` method populates two public structs:

```cpp
struct PhysicsConfig {
    glm::vec3 gravity              = {0.0f, -50.0f, 0.0f};
    float     jumpPower            = 30.0f;
    float     defaultRunSpeed      = 20.0f;
    float     defaultTurnSpeed     = 160.0f;
    float     npcTurnSpeed         = 80.0f;
    float     terrainSize          = 800.0f;
    float     defaultCapsuleRadius = 0.5f;
    float     defaultCapsuleHeight = 1.8f;
    float     defaultStepHeight    = 0.35f;
    float     defaultMass          = 70.0f;
    glm::vec3 defaultSpawnPosition = {100.0f, 3.0f, -80.0f};
    float     sprintMultiplier     = 4.5f;
};

struct ServerConfig {
    int   port         = 7777;
    int   maxClients   = 32;
    int   channelCount = 2;
    float tickInterval = 0.1f;   // 10 Hz
};
```

Every field has a sensible default so the engine boots even when a JSON file is missing.  The JSON parser uses `nlohmann::json::value()` which returns the default if the key is absent — there are no hard crashes from a missing field.

The `readVec3` helper supports both array `[x,y,z]` and object `{"x":…, "y":…, "z":…}` syntax, making config files flexible:

```cpp
static glm::vec3 readVec3(const nlohmann::json& j, const std::string& key,
                           const glm::vec3& fallback) {
    if (!j.contains(key)) return fallback;
    const auto& v = j[key];
    if (v.is_array() && v.size() >= 3)
        return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
    if (v.is_object())
        return {v.value("x", fallback.x), v.value("y", fallback.y),
                v.value("z", fallback.z)};
    return fallback;
}
```

### 2.2 Entity Spawning — EntityFactory & PrefabManager

#### PrefabManager

`PrefabManager` is a singleton cache of parsed JSON objects.  On boot it recursively scans `src/Resources/prefabs/` for `.json` files:

```cpp
void PrefabManager::loadAll(const std::string& resourceRoot) {
    const std::string prefabDir = resourceRoot + "/src/Resources/prefabs";
    for (const auto& entry : fs::recursive_directory_iterator(prefabDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            loadFile(entry.path().string());
    }
}
```

Each file's `"id"` field (or filename stem) becomes the lookup key.  The raw `nlohmann::json` object is stored — no custom struct — giving the factory complete freedom to inspect any key the prefab defines.

#### EntityFactory

`EntityFactory::spawn()` is the **single entry point** for creating entities in the ECS.  It is used by both players and NPCs:

```
EntityFactory::spawn(registry, prefabId, position, physicsSystem*)
       │
       ├── 1. Look up JSON from PrefabManager
       ├── 2. Create entt::entity
       ├── 3. Attach TransformComponent  (always)
       ├── 4. Attach NetworkIdComponent  (if "model_type" present)
       ├── 5. Attach InputStateComponent (runSpeed/turnSpeed from config or prefab override)
       ├── 6. Attach InputQueueComponent (always — needed for server ticking)
       ├── 7. Register Bullet character controller (if "physics" block present)
       ├── 8. Attach AIScriptComponent   (if "ai_script" present)
       └── 9. [CLIENT ONLY] Attach AssimpModelComponent (if "mesh" present)
```

Key code path for physics registration:

```cpp
if (physics && prefab.contains("physics")) {
    const auto& phys = prefab["physics"];
    float radius = phys.value("radius",
                       ConfigManager::get().physics.defaultCapsuleRadius);
    float height = phys.value("height",
                       ConfigManager::get().physics.defaultCapsuleHeight);
    physics->addCharacterController(entity, radius, height);
}
```

The factory reads capsule dimensions from the prefab's `"physics"` block, falling back to `ConfigManager` defaults if omitted.  This is how the same factory code handles a 0.5 m × 1.8 m player capsule and a potential 1.0 m × 3.0 m ogre capsule — entirely from data.

#### Player Spawn Flow (Server)

When a client connects, `ServerMain.cpp` spawns a player entity using the prefab pipeline and then assigns the runtime network ID:

```cpp
case ENET_EVENT_TYPE_CONNECT: {
    uint32_t newId = nextNetworkId++;
    peerToNetworkId[event.peer] = newId;

    glm::vec3 spawnPos = ConfigManager::get().physics.defaultSpawnPosition;
    if (terrainMgr.isAnyValid())
        spawnPos.y = terrainMgr.getHeight(spawnPos.x, spawnPos.z);

    auto entity = EntityFactory::spawn(registry, "player", spawnPos, &physicsSystem);
    networkIdToEntity[newId] = entity;

    // EntityFactory sets modelType and isNPC from the prefab but leaves id = 0.
    // Assign the actual server-assigned network ID here.
    if (auto* nid = registry.try_get<NetworkIdComponent>(entity))
        nid->id = newId;

    // ... send WelcomePacket, exchange SpawnPackets ...
}
```

### 2.3 Physics Loop — Authoritative Simulation

The server runs a fixed-rate tick loop driven by `std::chrono::steady_clock`.  Each tick:

1. **Poll ENet** — receive `PlayerInputPacket`s from clients, append them to the entity's `InputQueueComponent`.
2. **Tick NPC AI** — `ServerNPCManager::tick()` generates synthetic `PlayerInputPacket`s for each NPC and appends them to the same queue.
3. **Drain input queues through Bullet** — interleaved sub-stepping.

#### The Sub-Stepping Algorithm

The server uses a **transposed entity × input** loop to keep all entities in lock-step:

```
maxDepth = max queue depth across all entities
subDt    = tickInterval / maxDepth

for step in 0 .. maxDepth-1:
    for each entity with InputQueueComponent:
        if step < entity.queue.size():
            inp = entity.queue[step]
            ── SharedMovement::applyInput(inp, tc.position, tc.rotation)
            ── Compute XZ displacement, revert position
            ── physicsSystem.setEntityWalkDirection(entity, displacement)
            ── if inp.jump: physicsSystem.jumpCharacterController(entity)
        else:
            physicsSystem.setEntityWalkDirection(entity, vec3(0))

    physicsSystem.update(subDt)   // Step Bullet forward
    clampCharsToTerrain()         // Snap Y to heightmap surface
```

**Why this matters:** Because `SharedMovement::applyInput()` is a pure mathematical function shared between client and server, both sides compute identical horizontal displacements.  The server's Bullet world then produces the authoritative Y coordinate (gravity, jump arcs, stair stepping), while terrain clamping provides a hard floor.

```cpp
// src/Network/SharedMovement.cpp — horizontal displacement only
void SharedMovement::applyInput(const Network::PlayerInputPacket& input,
                                glm::vec3& position, glm::vec3& rotation) {
    rotation.y = input.cameraYaw;

    const float speed_val = runSpeed();  // reads ConfigManager
    float speed = 0.0f;
    if      (input.moveForward)  speed =  speed_val;
    else if (input.moveBackward) speed = -speed_val;

    float distance = speed * input.deltaTime;
    float sinY = std::sin(glm::radians(rotation.y));
    float cosY = std::cos(glm::radians(rotation.y));

    position.x += distance * sinY;
    position.z += distance * cosY;
    // Y intentionally omitted — Bullet owns the vertical axis.
}
```

After all inputs are consumed, the server broadcasts a `TransformSnapshot` for every entity:

```cpp
auto view = registry.view<TransformComponent, NetworkIdComponent>();
for (auto entity : view) {
    auto& tc  = view.get<TransformComponent>(entity);
    auto& nid = view.get<NetworkIdComponent>(entity);

    Network::TransformSnapshot snap;
    snap.networkId      = nid.id;
    snap.sequenceNumber = sequenceNum++;
    snap.timestamp      = serverTime;
    snap.position       = tc.position;
    snap.rotation       = tc.rotation;
    snap.lastProcessedInputSequence = nid.lastInputSeq;

    broadcast(server, Network::PacketType::TransformSnapshot, &snap, sizeof(snap));
}
```

The `lastProcessedInputSequence` field is critical — it tells the client *which* of its inputs the server has consumed, enabling precise reconciliation (see §4.3).

---

## 3. The Scripting Engine (Lua / Sol2)

### 3.1 LuaScriptEngine Implementation

`LuaScriptEngine` (in `src/Scripting/`) wraps a single `sol::state` (Sol2) and provides three operations: **init**, **loadScript**, and **tickAI**.

The engine is conditionally compiled via `#ifdef HAS_LUA`.  When Lua is not available, a stub class with no-op methods is compiled in its place, ensuring the rest of the codebase is unaffected.

#### Initialization

```cpp
void LuaScriptEngine::init(const std::string& resourceRoot) {
    resourceRoot_ = resourceRoot;

    // Standard Lua libraries
    lua_.open_libraries(sol::lib::base, sol::lib::math,
                        sol::lib::string, sol::lib::table);

    // Register C++ types as Lua usertypes
    lua_.new_usertype<LuaAIState>("AIState",
        "timer",     &LuaAIState::timer,
        "phase",     &LuaAIState::phase,
        "cameraYaw", &LuaAIState::cameraYaw
    );

    lua_.new_usertype<LuaAIResult>("AIResult",
        sol::constructors<LuaAIResult()>(),
        "moveForward",  &LuaAIResult::moveForward,
        "moveBackward", &LuaAIResult::moveBackward,
        "moveLeft",     &LuaAIResult::moveLeft,
        "moveRight",    &LuaAIResult::moveRight,
        "jump",         &LuaAIResult::jump,
        "cameraYaw",    &LuaAIResult::cameraYaw,
        "deltaTime",    &LuaAIResult::deltaTime
    );

    // Expose ConfigManager values as a global `config` table
    sol::table config = lua_.create_named_table("config");
    config["npcTurnSpeed"] = ConfigManager::get().physics.npcTurnSpeed;
    config["runSpeed"]     = ConfigManager::get().physics.defaultRunSpeed;
    config["turnSpeed"]    = ConfigManager::get().physics.defaultTurnSpeed;
    config["gravity"]      = ConfigManager::get().physics.gravity.y;
    config["jumpPower"]    = ConfigManager::get().physics.jumpPower;

    initialised_ = true;
}
```

### 3.2 C++ ↔ Lua Type Bindings

Two C++ structs are exposed to Lua via Sol2 usertypes:

| C++ Type | Lua Name | Purpose |
|---|---|---|
| `LuaAIState` | `AIState` | Mutable per-NPC state (`timer`, `phase`, `cameraYaw`) passed by reference into Lua.  Changes persist across ticks. |
| `LuaAIResult` | `AIResult` | Immutable return value from a tick function — movement flags + yaw that the server converts into a `PlayerInputPacket`. |

The global `config` table gives scripts read access to `ConfigManager` values (e.g. `config.npcTurnSpeed`), ensuring Lua scripts stay data-driven and never hardcode physics constants.

### 3.3 Execution Flow in ServerNPCManager

```
ServerNPCManager::initLua(resourceRoot)
       │
       ├── LuaScriptEngine::init(resourceRoot)
       │
       ├── For each registered NPC:
       │     ├── Find the prefab with a matching AIComponent.script name
       │     ├── Read the prefab's "ai_script" path (e.g. "scripts/ai/wander.lua")
       │     └── LuaScriptEngine::loadScript(path)
       │
       └── Set luaReady_ = true  (if at least one script loaded)

ServerNPCManager::tick(dt, outInputs)
       │
       └── For each registered NPC (id, scriptType):
             │
             ├── [Lua path]  if luaReady_ && hasScript(scriptType):
             │     ├── pkt = luaEngine_.tickAI(scriptType, id, dt, luaState)
             │     ├── Sync luaState → C++ NPCAIState (for inspection/fallback)
             │     └── outInputs[id] = pkt
             │
             └── [C++ fallback]  else:
                   ├── Execute built-in GuardAI / WanderAI C++ logic
                   └── outInputs[id] = pkt
```

#### tickAI internals

```cpp
Network::PlayerInputPacket LuaScriptEngine::tickAI(
    const std::string& scriptName, uint32_t entityId,
    float dt, LuaAIState& state)
{
    Network::PlayerInputPacket pkt{};
    pkt.deltaTime = dt;

    sol::protected_function fn = lua_[scriptName];
    if (!fn.valid()) return pkt;

    auto callResult = fn(state, dt);        // ← Call Lua: WanderAI(state, dt)
    if (!callResult.valid()) { /* log error */ return pkt; }

    LuaAIResult res = callResult.get<LuaAIResult>();  // ← Sol2 auto-converts

    pkt.moveForward  = res.moveForward;
    pkt.moveBackward = res.moveBackward;
    pkt.moveLeft     = res.moveLeft;
    pkt.moveRight    = res.moveRight;
    pkt.jump         = res.jump;
    pkt.cameraYaw    = res.cameraYaw;

    return pkt;
}
```

Because the return type is a `PlayerInputPacket`, the NPC's movement is injected into the same `InputQueueComponent` → `SharedMovement` → Bullet pipeline as a real player.  The server does not differentiate between human and AI-controlled entities at the physics layer.

---

## 4. Client-Server Interaction (The Pipeline)

### 4.1 Input Flow — From Keyboard to Wire

```
┌──────────┐     ┌───────────┐     ┌──────────────┐     ┌────────┐
│ Keyboard │ ──► │ InputMaster│ ──► │ NetworkSystem│ ──► │ ENet   │
│ (GLFW)   │     │ actionMap  │     │ .update()    │     │ (wire) │
└──────────┘     └───────────┘     └──────────────┘     └────────┘
```

1. **GLFW** fires key callbacks.  `InputMaster` stores raw key states.

2. At boot, `InputMaster::loadBindings()` reads `controls.json` to populate an **action map** (string → GLFW key code):

   ```json
   {
     "bindings": {
       "MoveForward":  "W",
       "MoveBackward": "S",
       "Jump":         "Space",
       "Sprint":       "Tab"
     }
   }
   ```

3. Game systems call `InputMaster::isActionDown("MoveForward")` instead of raw `isKeyDown(W)`, enabling full rebinding from data.

4. `NetworkSystem::update()` samples the action map and builds a `PlayerInputPacket`:

   ```cpp
   Network::PlayerInputPacket input;
   input.sequenceNumber = ++inputSequenceNumber_;
   input.deltaTime      = deltaTime;
   input.cameraYaw      = localPlayer_->getRotation().y;
   input.moveForward    = InputMaster::isActionDown("MoveForward");
   input.moveBackward   = InputMaster::isActionDown("MoveBackward");
   input.jump           = InputMaster::isActionDown("Jump");
   // A/D are turn keys — their effect is captured in cameraYaw, not sent as flags.
   input.moveLeft  = false;
   input.moveRight = false;
   ```

5. The packet is serialised and sent over ENet as unreliable UDP.

**Critical design point:** The client **never** sends its position or rotation over the wire.  Only raw button states and the camera yaw are transmitted.  This makes teleport-hacking physically impossible at the protocol level.

### 4.2 Server Authority

On receiving a `PlayerInputPacket`, the server appends it to the entity's `InputQueueComponent`:

```cpp
auto& queue = registry.get<InputQueueComponent>(entity);
queue.inputs.push_back(input);
```

On the next tick, the queue is drained through `SharedMovement::applyInput()` and Bullet Physics (see §2.3).  The resulting position is the **authoritative truth**.

Each broadcast `TransformSnapshot` includes `lastProcessedInputSequence` — the highest input sequence number the server has consumed for that entity:

```cpp
struct TransformSnapshot {
    uint32_t networkId;
    uint32_t sequenceNumber;
    float    timestamp;
    glm::vec3 position;
    glm::vec3 rotation;
    uint32_t lastProcessedInputSequence;  // ← key for reconciliation
};
```

### 4.3 Reconciliation — "Real Reconcile Triggered"

The client maintains a rolling `localHistory_` buffer — a record of `{sequenceNumber, position}` pairs representing where the client was when it sent each input.

```cpp
struct PlayerHistory {
    uint32_t  sequenceNumber;
    glm::vec3 position;
};
std::vector<PlayerHistory> localHistory_;   // max 100 entries
```

When a `TransformSnapshot` arrives for the local player, the reconciliation algorithm executes:

```
1. FIND the localHistory_ entry whose sequenceNumber matches
   snapshot.lastProcessedInputSequence.
   This is where the client WAS when the server processed that input.

2. COMPUTE the XZ difference:
   diff = snapshot.position - historicalPos
   diff.y = 0   // Y is excluded — owned by client physics

3. CHECK if distSq = dot(diff, diff) > kReconcileThreshSq (0.1)

4. CORRECT by applying the mathematical error to the client's
   CURRENT position (not the historical one):
   correctedPos = currentClientPos + diff
   correctedPos.y = currentClientPos.y   // preserve client Y

5. WARP the player's physics body to the corrected position.
```

In code:

```cpp
// 1. Find where the client WAS when the server processed this input
auto it = std::find_if(localHistory_.begin(), localHistory_.end(),
    [&](const PlayerHistory& h) {
        return h.sequenceNumber == snapshot.lastProcessedInputSequence;
    });

glm::vec3 historicalPos = currentClientPos;
if (it != localHistory_.end()) {
    historicalPos = it->position;
    localHistory_.erase(localHistory_.begin(), it);  // Prune older history
}

// 2. Compare Server Past vs Client Past (XZ only)
glm::vec3 diff = snapshot.position - historicalPos;
diff.y = 0.0f;

// 3. Threshold check
float distSq = glm::dot(diff, diff);
if (distSq > kReconcileThreshSq) {
    // 4. Apply error to CURRENT position
    glm::vec3 correctedPos = currentClientPos + diff;
    correctedPos.y = currentClientPos.y;

    localPlayer_->setPosition(correctedPos);
    physicsSystem_->warpPlayer(correctedPos);

    // 5. Log
    std::cout << "[NetworkSystem] Real Reconcile Triggered.\n"
              << "   -> Client Hist Pos: (" << historicalPos.x << ", "
              << historicalPos.z << ")\n"
              << "   -> Server Snap Pos: (" << snapshot.position.x << ", "
              << snapshot.position.z << ")\n"
              << "   -> XZ Discrepancy : (" << diff.x << ", "
              << diff.z << ")\n";
}
```

**Why Y is excluded:** The Y axis is owned by the client's local physics simulation (`PlayerMovementSystem` terrain clamping).  The server terrain-clamps independently but may differ by sub-pixel amounts due to floating-point timing differences.  Reconciling Y would cause visible vertical jitter.

**Why we add the diff to CURRENT position:** Between the time the server processed input N and now, the client has continued to predict several additional frames (inputs N+1 … N+K).  Hard-snapping to the server's historical position would throw away those predictions.  Instead, we compute the *error* at the historical moment and shift the current position by that error — preserving all subsequent prediction while correcting the root discrepancy.

---

## 5. Data Structure Examples

### 5.1 World Config (`world_config.json`)

```jsonc
{
  // Human-readable comment for editors
  "_comment": "world_config.json — Loaded by ConfigManager on boot.
               Both client and server read this file.",

  // Where newly connected players spawn (feet position).
  // The server terrain-clamps Y at runtime.
  "default_spawn_position": { "x": 100.0, "y": 3.0, "z": -80.0 },

  "physics": {
    "gravity":             [0.0, -50.0, 0.0],  // vec3 — array syntax
    "jump_power":          30.0,
    "default_run_speed":   20.0,      // units/sec — horizontal walk speed
    "default_turn_speed":  160.0,     // degrees/sec — player turn rate
    "npc_turn_speed":      80.0,      // degrees/sec — AI turn rate
    "terrain_size":        800.0,     // world units per terrain tile

    // Capsule defaults for any entity without per-prefab overrides
    "default_capsule_radius": 0.5,
    "default_capsule_height": 1.8,
    "default_step_height":    0.35,
    "default_mass":           70.0,

    "sprint_multiplier":   4.5        // multiplied with runSpeed when sprinting
  },

  "server": {
    "port":           7777,
    "max_clients":    32,
    "channel_count":  2,
    "tick_interval":  0.1             // seconds → 10 Hz tick rate
  }
}
```

**Key architectural point:** Every field has a compiled default in `PhysicsConfig` / `ServerConfig`.  The JSON file *overrides* defaults — omitting a key silently falls back to the coded value.

### 5.2 Prefab JSON (`prefabs/npc_wanderer.json`)

```jsonc
{
  // Unique prefab identifier — used by EntityFactory::spawn("npc_wanderer", ...)
  "id": "npc_wanderer",
  "description": "A wandering NPC that walks forward for 3 seconds, then turns.",

  // Sent in SpawnPacket so the client knows which mesh to render
  "model_type": "npc_wanderer",

  // Client-only: path to the 3D model loaded by Assimp
  "mesh": "models/wanderer.glb",

  // Bullet Physics capsule — overrides ConfigManager defaults
  "physics": {
    "type":        "kinematic_character",
    "radius":      0.5,
    "height":      1.8,
    "mass":        70.0,
    "step_height": 0.35
  },

  // Skeletal animation controller (client)
  "animation_controller": "animations/humanoid.json",

  // Lua AI script to load — path relative to src/Resources/
  "ai_script": "scripts/ai/wander.lua",

  // ECS component overrides and metadata
  "components": {
    "NetworkSyncComponent": {
      "interpolation_delay": 0.20,
      "max_buffer_size": 20
    },
    "AIComponent": {
      // Logical function name that LuaScriptEngine::tickAI() calls
      "script": "WanderAI"
    }
  }
}
```

**How the fields connect:**

| Prefab Field | Read By | Effect |
|---|---|---|
| `"id"` | `PrefabManager` | Lookup key for `EntityFactory::spawn()` |
| `"model_type"` | `EntityFactory` | Written to `NetworkIdComponent.modelType` → sent in `SpawnPacket` |
| `"mesh"` | `EntityFactory` (client only) | Written to `AssimpModelComponent.meshPath` for GPU loading |
| `"physics"` | `EntityFactory` | Passed to `PhysicsSystem::addCharacterController()` |
| `"ai_script"` | `EntityFactory` + `ServerNPCManager` | `EntityFactory` stores in `AIScriptComponent`; `ServerNPCManager::initLua()` loads the file into Lua |
| `"components.AIComponent.script"` | `ServerNPCManager` | The global Lua function name to call on each tick |

### 5.3 Lua AI Script (`scripts/ai/wander.lua`)

```lua
-- scripts/ai/wander.lua
--
-- WanderAI — walk forward for 3 seconds, turn for 1 second, repeat.
--
-- Called by C++ as: WanderAI(state, dt)
--   state : AIState usertype (mutable)  — { timer, phase, cameraYaw }
--   dt    : float                       — tick delta time (seconds)
-- Returns: AIResult usertype            — { moveForward, cameraYaw, ... }

function WanderAI(state, dt)
    local result = AIResult.new()       -- Construct a C++ LuaAIResult via Sol2
    result.deltaTime = dt

    state.timer = state.timer + dt      -- Accumulate time in current phase

    if state.phase == 0 then
        -- Phase 0: Walk forward
        result.moveForward = true
        if state.timer >= 3.0 then
            state.timer = 0.0
            state.phase = 1             -- Transition to turning phase
        end
    elseif state.phase == 1 then
        -- Phase 1: Turn in place
        state.cameraYaw = state.cameraYaw + config.npcTurnSpeed * dt
        --                                  ^^^^^^^^^^^^^^^^^^^^^^^^
        --                                  Read from ConfigManager via the
        --                                  global `config` table (see §3.1)
        if state.timer >= 1.0 then
            state.timer = 0.0
            state.phase = 0             -- Back to walking
        end
    else
        -- Safety reset
        state.phase = 0
        state.timer = 0.0
    end

    result.cameraYaw = state.cameraYaw  -- Server uses this as the entity's yaw
    return result                       -- Returned to C++ as LuaAIResult
end
```

**Execution flow summary:**

1. The Lua function receives a mutable `AIState` and a `dt` float.
2. It constructs an `AIResult` with `AIResult.new()` (Sol2 constructor binding).
3. It modifies `state` (persists across ticks) and populates `result` (consumed once).
4. The C++ `tickAI()` converts the returned `LuaAIResult` into a `PlayerInputPacket`.
5. The packet is appended to the NPC's `InputQueueComponent`.
6. On the next tick, the server's physics loop processes it identically to a human player's input.

---

## Appendix: Directory Map

```
src/
├── Config/
│   ├── ConfigManager.h/.cpp     — Singleton JSON config loader
│   ├── PrefabManager.h/.cpp     — Prefab JSON cache
│   └── EntityFactory.h/.cpp     — Data-driven entity spawner
├── ECS/Components/
│   ├── TransformComponent.h     — Position + rotation + scale
│   ├── NetworkIdComponent.h     — Wire ID + model type + NPC flag
│   ├── InputStateComponent.h    — Per-entity movement state
│   ├── InputQueueComponent.h    — Server-side input buffer
│   ├── AIScriptComponent.h      — Lua script path + name
│   └── AssimpModelComponent.h   — Client-only mesh reference
├── Network/
│   ├── NetworkPackets.h         — POD packet structs + serialise<T>()
│   └── SharedMovement.h/.cpp    — Deterministic XZ movement math
├── Physics/
│   └── PhysicsSystem.h/.cpp     — Bullet wrapper, character controllers
├── Scripting/
│   └── LuaScriptEngine.h/.cpp   — Sol2 Lua bridge
├── Server/
│   ├── ServerMain.cpp           — Headless server entry point + tick loop
│   └── ServerNPCManager.h/.cpp  — NPC AI management + Lua integration
├── Engine/
│   ├── Engine.h/.cpp            — Client game loop + system orchestration
│   ├── NetworkSystem.h/.cpp     — Client networking + reconciliation
│   └── PlayerMovementSystem.cpp — Client-side prediction + terrain clamp
├── Input/
│   └── InputMaster.h/.cpp       — GLFW input + action bindings
└── Resources/
    ├── world_config.json        — Physics, server, spawn settings
    ├── client_settings.json     — Window, FOV, camera settings
    ├── controls.json            — Rebindable input action map
    ├── prefabs/
    │   ├── player.json
    │   ├── npc_guard.json
    │   └── npc_wanderer.json
    └── scripts/ai/
        ├── guard.lua
        └── wander.lua
```

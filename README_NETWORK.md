# Phase 1 — Authoritative Mock Server & Entity Interpolation

## What Was Built

This PR implements **Phase 1** of the Authoritative Server / Dumb Client
networking architecture.  No sockets are opened.  Everything runs locally in
the same process, but the data flow exactly mirrors what a real server
connection will do.

### New Files

| File | Purpose |
|---|---|
| `src/Network/NetworkPackets.h` | **Data Contract** — `TransformSnapshot` struct |
| `src/Network/MockServer.h/.cpp` | **10 Hz Authoritative Server** simulator (singleton) |
| `src/Entities/Components/NetworkSyncComponent.h/.cpp` | **Client-side interpolation** component |
| `src/Engine/NetworkSystem.h/.cpp` | **ISystem** that drives the network layer each frame |

---

## How to Run

There is nothing extra to do.  The mock network layer is integrated into the
existing `Engine` startup sequence.  Run the engine as you normally would:

```bash
# macOS / Linux
cmake -S . -B build && cmake --build build
./build/engine
```

On startup, the engine prints:

```
[Engine] NetworkSyncComponent entity spawned — orbiting at (100, 3, -80) r=20
```

A **second entity** (a lamp model) will appear near the player's start
position.  It circles the point `(100, 3, -80)` with a radius of 20 world
units, rotating to face its direction of travel, updating smoothly at the
render frame rate despite only receiving server data every 100 ms.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        Game Loop (Engine::run)               │
│                                                             │
│  InputDispatcher → PhysicsSystem → InputSystem              │
│       → StreamingSystem                                     │
│       → NetworkSystem  ← you are here                       │
│             │                                               │
│             ├─ MockServer::update(dt)  (10 Hz ticks)        │
│             │       └─ TransformSnapshot ──► buffer         │
│             └─ networkEntity_->updateComponents(dt)         │
│                     └─ NetworkSyncComponent::update(dt)     │
│                             └─ glm::mix / glm::slerp        │
│                                     └─ Entity::setPosition  │
│       → RenderSystem  (sees updated position)               │
│       → AnimationSystem → UISystem                          │
└─────────────────────────────────────────────────────────────┘
```

### Key Concepts

#### `TransformSnapshot` (Data Contract)
```cpp
struct TransformSnapshot {
    uint32_t  sequenceNumber; // monotonic counter — drop out-of-order packets
    float     timestamp;      // server simulation clock (seconds)
    glm::vec3 position;       // world-space XYZ
    glm::vec3 rotation;       // Euler angles in degrees (matches Entity API)
};
```
This is the **only** payload that will be sent/received over the network in
Phase 2.  Nothing else crosses the wire.

#### `MockServer` (10 Hz Authoritative Tick)
The singleton fires a tick every `kTickInterval` (100 ms) and pushes a new
`TransformSnapshot` into every registered `NetworkSyncComponent`.  The path
generator currently produces a circular orbit; see **Extending the Path**
below.

#### `NetworkSyncComponent` (Entity Interpolation)
The component maintains a **playback clock** that lags `kInterpolationDelay`
(150 ms) behind the newest received snapshot.  Each render frame it:

1. Advances its clock by `deltaTime`.
2. Computes `targetTime = renderTime - kInterpolationDelay`.
3. Finds the two snapshots bracketing `targetTime`.
4. `glm::mix` LERP for position, `glm::slerp` SLERP for rotation.
5. Calls `Entity::setPosition` / `Entity::setRotation` with the result.

**Edge cases handled:**
- Buffer empty → entity not moved (no crash).
- Only 1 snapshot → hold at that position.
- Buffer starvation (newest snapshot is older than `targetTime`) →
  extrapolate linearly for up to 2× the inter-tick interval, then hold.

---

## Input / Output Data Contract

### What MockServer *sends* (simulating a server packet)

```cpp
Network::TransformSnapshot snap;
snap.sequenceNumber = ...;         // uint32, monotonic
snap.timestamp      = serverTime_; // float, seconds since server start
snap.position       = {x, y, z};  // world position
snap.rotation       = {rx, ry, rz}; // Euler degrees
```

### What `NetworkSyncComponent` *receives*

```cpp
component->pushSnapshot(snap);
```

Call this once per received packet.  Thread-safe in Phase 1 (single-threaded).
In Phase 2, add a `std::mutex` around the `buffer_` deque.

### What the component *outputs* (to its owning Entity)

Every frame:
```cpp
entity_->setPosition(interpolated_position);  // glm::vec3
entity_->setRotation(interpolated_euler_deg); // glm::vec3 (degrees)
```

---

## Extending to New Movement Paths

### Change the mock server path

In `MockServer.cpp`, edit `generatePosition(float t)` and
`generateRotation(float t)`.  The parameter `t` is the server's elapsed
simulation time in seconds.

```cpp
// Current: circle
glm::vec3 MockServer::generatePosition(float t) const {
    return { kOrbitCentreX + kOrbitRadius * std::cos(t * kAngularSpeed),
             kOrbitCentreY,
             kOrbitCentreZ + kOrbitRadius * std::sin(t * kAngularSpeed) };
}

// Example: straight-line patrol
glm::vec3 MockServer::generatePosition(float t) const {
    float x = kOrbitCentreX + 50.0f * std::sin(t * 0.5f);
    return { x, kOrbitCentreY, kOrbitCentreZ };
}

// Example: read from a waypoint list
glm::vec3 MockServer::generatePosition(float t) const {
    static const std::vector<glm::vec3> waypoints = { ... };
    int   idx  = (int)(t / 2.0f) % (int)waypoints.size();
    float frac = std::fmod(t, 2.0f) / 2.0f;
    return glm::mix(waypoints[idx], waypoints[(idx+1) % waypoints.size()], frac);
}
```

### Add a second network entity

```cpp
// In Engine::buildSystems() or anywhere before the main loop:
auto* entity2 = new Entity(someModel, nullptr, startPos, glm::vec3(0), 1.0f);
auto* comp2   = entity2->addComponent<NetworkSyncComponent>();
MockServer::instance().registerComponent(comp2);
entities.push_back(entity2);
networkSystem->addEntity(entity2);  // if you keep a pointer to NetworkSystem
```

### Add a new field to the packet

1. Add the field to `TransformSnapshot` in `NetworkPackets.h`.
2. Generate and assign it in `MockServer::dispatchSnapshot()`.
3. Read and apply it in `NetworkSyncComponent::update()`.
4. No other files need changes.

### Change the tick rate

```cpp
// MockServer.h
static constexpr float kTickInterval = 0.05f; // 20 Hz
```

### Change the interpolation delay

```cpp
// NetworkSyncComponent.h
static constexpr float kInterpolationDelay = 0.1f; // 100 ms (one tick behind)
```

A delay of 1–2× the tick interval provides the best trade-off between
latency and smoothness.

---

## Phase 2 Integration Points

When you wire in real UDP sockets (ENet or raw UDP):

1. **Replace `MockServer`** with a real `NetworkClient` that reads bytes off
   a socket and calls `NetworkSyncComponent::pushSnapshot()`.
2. **`TransformSnapshot` is the packet format** — add serialisation (e.g.
   `std::memcpy` into a fixed-size buffer, or use a schema like FlatBuffers).
3. **Client Prediction** (Phase 3): move the *local* player immediately on
   `W/A/S/D` input, timestamp the input, send it to the server, and store it
   in an `InputHistory` deque.  When the server sends back an authoritative
   position that disagrees, snap to it and replay inputs from the history.
4. **`pushSnapshot` is thread-safe** once you add a mutex — the receive
   thread can push; the render thread pops/interpolates.

---

## Tuning Reference

| Constant | Location | Default | Effect |
|---|---|---|---|
| `kTickInterval` | `MockServer.h` | `0.1f` | Server tick rate (seconds) |
| `kOrbitRadius` | `MockServer.h` | `20.0f` | Circle radius (world units) |
| `kAngularSpeed` | `MockServer.h` | `0.8f` | rad/s around orbit centre |
| `kInterpolationDelay` | `NetworkSyncComponent.h` | `0.15f` | Render lag behind server (s) |

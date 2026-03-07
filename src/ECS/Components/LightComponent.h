// src/ECS/Components/LightComponent.h
//
// Pure POD EnTT component that carries all lighting data inline.
// One registry entity is created per scene light.
//
// Design (Phase 2 Step 3):
//   Light data (position, color, attenuation) lives directly in this struct
//   rather than behind a heap-allocated Light*.  The position field doubles as
//   the canonical world-space position; a future step may move it into a
//   companion TransformComponent.
//
// Lifetime: component is stored by value inside the EnTT registry storage.
//   No manual delete is required in Engine::shutdown() — the Light is
//   automatically destroyed when the registry entity is erased.
//
// Usage in Systems:
//   auto view = registry.view<LightComponent>();
//   for (auto [e, lc] : view.each()) {
//       lights.push_back(&lc.light);  // stable pointer into registry storage
//   }

#ifndef ECS_LIGHTCOMPONENT_H
#define ECS_LIGHTCOMPONENT_H

#include "../Entities/Light.h"

struct LightComponent {
    Light light; ///< all light data stored by value — no heap allocation
};

#endif // ECS_LIGHTCOMPONENT_H

//
// IComponent.h — Abstract base for all Entity components.
//
// Components are attached to an Entity via Entity::addComponent<T>().
// Each component holds a back-pointer to its owning Entity so it can
// be initialised with entity context.
//
// Phase 2 Step 3: The pure-virtual update() has been removed.  Per-frame
// logic now lives exclusively in ISystem subclasses (PhysicsSystem,
// InputSystem, NetworkSystem, etc.) that iterate EnTT registry views.
// Components are pure data — they store state but contain no tick logic.
//
// Circular-include note: Entity.h includes this header; therefore this
// header must forward-declare Entity rather than including Entity.h.
//

#ifndef ENGINE_ICOMPONENT_H
#define ENGINE_ICOMPONENT_H

// Use the official forward-declaration header so we get nlohmann::json
// without triggering a name-collision between our forward-decl and the
// library's own  `using json = basic_json<>`.
#include <nlohmann/json_fwd.hpp>

class Entity;

class IComponent {
public:
    virtual ~IComponent() = default;

    /// Called once after the component is attached to its entity.
    /// Override to perform one-time setup (e.g. subscribe to events).
    virtual void init() {}

    /// Data-driven initialisation: load component variables from a JSON object.
    /// Override in concrete components to support prefab-based instantiation.
    /// The default implementation is a no-op so existing components remain valid.
    virtual void initFromJson(const nlohmann::json& /*j*/) {}

    /// Store the owning entity pointer so the component can manipulate it.
    void setEntity(Entity* entity) { entity_ = entity; }

protected:
    Entity* entity_ = nullptr;
};

#endif // ENGINE_ICOMPONENT_H

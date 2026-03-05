//
// IComponent.h — Abstract base for all Entity components.
//
// Components are attached to an Entity via Entity::addComponent<T>().
// Each component holds a back-pointer to its owning Entity so it can
// read and mutate entity state (position, rotation, etc.) each frame.
//
// Circular-include note: Entity.h includes this header; therefore this
// header must forward-declare Entity rather than including Entity.h.
//

#ifndef ENGINE_ICOMPONENT_H
#define ENGINE_ICOMPONENT_H

class Entity;

class IComponent {
public:
    virtual ~IComponent() = default;

    /// Called once after the component is attached to its entity.
    /// Override to perform one-time setup (e.g. subscribe to events).
    virtual void init() {}

    /// Called every frame with the current frame delta-time (seconds).
    virtual void update(float deltaTime) = 0;

    /// Store the owning entity pointer so the component can manipulate it.
    void setEntity(Entity* entity) { entity_ = entity; }

protected:
    Entity* entity_ = nullptr;
};

#endif // ENGINE_ICOMPONENT_H

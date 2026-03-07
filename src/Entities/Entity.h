//
// Created by Joseph Alai on 7/1/21.
//

#ifndef ENGINE_ENTITY_H
#define ENGINE_ENTITY_H

#include "glm/glm.hpp"
#include "../Models/TexturedModel.h"
#include "../BoundingBox/BoundingBox.h"
#include "../Interfaces/Interactive.h"
#include "../BoundingBox/BoundingBoxIndex.h"
#include "Components/IComponent.h"
#include "ComponentPool.h"
#include <vector>
#include <memory>
#include <functional>
#include <entt/entt.hpp>
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/RenderComponent.h"
#include "../ECS/Components/ColliderComponent.h"
#include "../ECS/Components/MaterialDataComponent.h"

class Entity : public Interactive {
public:

    /**
     * @brief ECS-backed constructor.  Creates an entt entity and emplaces
     *        TransformComponent, RenderComponent, ColliderComponent and
     *        MaterialDataComponent into the supplied registry.
     *
     * @param registry  The engine-level entt::registry that owns the data.
     * @param model     TexturedModel (RawModel + Texture).
     * @param box       Bounding box used for picking/collision.
     * @param position  Initial world position.
     * @param rotation  Initial Euler rotation (degrees).
     * @param scale     Uniform scale factor.
     */
    explicit Entity(entt::registry& registry, TexturedModel *model, BoundingBox *box,
                    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3 rotation = glm::vec3(0),
                    float scale = 1.0f);

    /**
     * @brief ECS-backed constructor with texture atlas index.
     *
     * @param registry     The engine-level entt::registry that owns the data.
     * @param model        TexturedModel (RawModel + Texture).
     * @param box          Bounding box used for picking/collision.
     * @param textureIndex Atlas row index for texture atlases.
     * @param position     Initial world position.
     * @param rotation     Initial Euler rotation (degrees).
     * @param scale        Uniform scale factor.
     */
    explicit Entity(entt::registry& registry, TexturedModel *model, BoundingBox *box,
                    int textureIndex,
                    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3 rotation = glm::vec3(0),
                    float scale = 1.0f);

    BoundingBox *getBoundingBox() const override;

    void setBoundingBox(BoundingBox *box) override;

    TexturedModel *getModel();

    /**
     * @brief gets the yOffset for the texture
     * @return
     */
    float getTextureYOffset();

    /**
     * @brief gets the xOffset for the texture
     * @return
     */
    float getTextureXOffset();

    glm::vec3 &getPosition() override;

    void setPosition(glm::vec3 translate) override;

    void increasePosition(glm::vec3 translate) override;

    glm::vec3 getRotation() override;

    void rotate(glm::vec3 rotate) override;

    void setRotation(glm::vec3 rotate) override;

    void increaseScale(float scaleSize) override;

    void setScale(float scaleSize) override;

    float getScale() const override;

    void setTransformation(glm::vec3 translate, glm::vec3 rotate, float scalar) override;

    Material getMaterial() const;

    void setMaterial(Material material);

    bool hasMaterial() const;

    void activateMaterial();

    void disableMaterial();

    /// Access the underlying ECS entity handle.
    entt::entity getHandle() const { return handle_; }

    /// Access the registry this entity belongs to.
    entt::registry& getRegistry() { return *registry_; }

    // -------------------------------------------------------------------------
    // Component container
    // -------------------------------------------------------------------------

    /// Destructor — releases all pooled components and destroys the ECS entity.
    virtual ~Entity();

    /// Allocate a component of type T from its global ComponentPool, attach it
    /// to this entity, and call its init() method.  Returns a raw pointer for
    /// convenience (pool manages the memory; Entity manages the lifecycle).
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        T* ptr = ComponentPool<T>::global().allocate();
        // Re-construct with provided args via assignment (T must be default-constructible).
        if constexpr (sizeof...(Args) > 0) {
            *ptr = T(std::forward<Args>(args)...);
        }
        ptr->setEntity(this);
        ptr->init();
        components_.push_back({
            static_cast<IComponent*>(ptr),
            [ptr]() { ComponentPool<T>::global().release(ptr); }
        });
        return ptr;
    }

    /// Return a raw pointer to the first component of type T, or nullptr.
    template<typename T>
    T* getComponent() {
        for (auto& entry : components_) {
            if (T* ptr = dynamic_cast<T*>(entry.ptr)) {
                return ptr;
            }
        }
        return nullptr;
    }

    /// Tick all attached components.  Call once per frame.
    virtual void updateComponents(float deltaTime) {
        for (auto& entry : components_) {
            entry.ptr->update(deltaTime);
        }
    }

private:
    // -----------------------------------------------------------------------
    // ECS handle — all data lives in the registry components above.
    // -----------------------------------------------------------------------
    entt::entity    handle_;
    entt::registry* registry_;

    // -----------------------------------------------------------------------
    // ComponentEntry — pairs an IComponent raw pointer with a type-erased
    // release function so Entity can return components to their pools without
    // knowing the concrete type at destruction time.
    // -----------------------------------------------------------------------
    struct ComponentEntry {
        IComponent*           ptr      = nullptr;
        std::function<void()> releaser;
    };

    std::vector<ComponentEntry> components_;

};

#endif //ENGINE_ENTITY_H

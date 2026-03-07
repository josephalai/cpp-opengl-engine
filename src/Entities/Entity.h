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
    // ECS Component interface — delegates directly to entt::registry
    // -------------------------------------------------------------------------

    /// Destructor — destroys the ECS entity and all its components.
    virtual ~Entity();

    /// Emplace a component of type T into the registry for this entity.
    /// Returns a raw pointer into registry storage for convenience.
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto& comp = registry_->emplace<T>(handle_, std::forward<Args>(args)...);
        return &comp;
    }

    /// Return a raw pointer to the component of type T, or nullptr.
    template<typename T>
    T* getComponent() {
        return registry_->try_get<T>(handle_);
    }

private:
    // -----------------------------------------------------------------------
    // ECS handle — all data lives in the registry components above.
    // -----------------------------------------------------------------------
    entt::entity    handle_;
    entt::registry* registry_;
};

#endif //ENGINE_ENTITY_H

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
#include <vector>
#include <memory>

class Entity : public Interactive {
protected:
    TexturedModel *model;

    BoundingBox *box;

    glm::vec3 position;

    glm::vec3 rotation;

    float scale;

    int textureIndex = 0;

    Material material = {0.1, 0.9};

    bool textureActivated = false;
public:

    /**
     * @brief Entity stores the TexturedModel (RawModel & Texture), and stores vectors
     *        to manipulate its' vertex, rotation, fontSize, (transformation).  It also
     *        stores the textureOffsets for textures in case there is a texture atlas.
     *
     * @param model
     * @param position
     * @param rotation
     * @param scale
     */
    explicit Entity(TexturedModel *model, BoundingBox *box, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3 rotation = glm::vec3(0),
                    float scale = 1.0f);

    /**
      * @brief Entity stores the TexturedModel (RawModel & Texture), and stores vectors
      *        to manipulate its' vertex, rotation, fontSize, (transformation).  It also
      *        stores the textureOffsets and kBboxIndices for textures in case there is a
      *        texture atlas.
      *
      * @param model
      * @param position
      * @param rotation
      * @param scale
      */
    explicit Entity(TexturedModel *model, BoundingBox *box, int textureIndex,
                    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 rotation = glm::vec3(0),
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

    // -------------------------------------------------------------------------
    // Component container
    // -------------------------------------------------------------------------

    /// Construct a component of type T in-place, attach it to this entity,
    /// and call its init() method.  Returns a raw pointer for convenience
    /// (lifetime is managed by the entity's components_ vector).
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = comp.get();
        comp->setEntity(this);
        comp->init();
        components_.push_back(std::move(comp));
        return ptr;
    }

    /// Return a raw pointer to the first component of type T, or nullptr.
    template<typename T>
    T* getComponent() {
        for (auto& comp : components_) {
            if (T* ptr = dynamic_cast<T*>(comp.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    /// Tick all attached components.  Call once per frame.
    virtual void updateComponents(float deltaTime) {
        for (auto& comp : components_) {
            comp->update(deltaTime);
        }
    }

private:
    std::vector<std::unique_ptr<IComponent>> components_;

};

#endif //ENGINE_ENTITY_H

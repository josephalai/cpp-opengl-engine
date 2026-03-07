//
// Created by Joseph Alai on 7/30/21.
//
#include "Entity.h"

Entity::Entity(entt::registry& registry, TexturedModel *model, BoundingBox *box,
               glm::vec3 position, glm::vec3 rotation, float scale)
    : registry_(&registry)
{
    handle_ = registry_->create();
    registry_->emplace<TransformComponent>(handle_, position, rotation, scale);
    registry_->emplace<RenderComponent>(handle_, model, 0);
    registry_->emplace<ColliderComponent>(handle_, box);
    registry_->emplace<MaterialDataComponent>(handle_);
}

Entity::Entity(entt::registry& registry, TexturedModel *model, BoundingBox *box,
               int textureIndex, glm::vec3 position, glm::vec3 rotation, float scale)
    : registry_(&registry)
{
    handle_ = registry_->create();
    registry_->emplace<TransformComponent>(handle_, position, rotation, scale);
    registry_->emplace<RenderComponent>(handle_, model, textureIndex);
    registry_->emplace<ColliderComponent>(handle_, box);
    registry_->emplace<MaterialDataComponent>(handle_);
}

Entity::~Entity() {
    if (registry_ != nullptr && registry_->valid(handle_)) {
        registry_->destroy(handle_);
    }
}

BoundingBox *Entity::getBoundingBox() const {
    const auto& reg = std::as_const(*registry_);
    if (reg.all_of<ColliderComponent>(handle_)) {
        return reg.get<ColliderComponent>(handle_).box;
    }
    return nullptr;
}

void Entity::setBoundingBox(BoundingBox *box) {
    if (registry_->all_of<ColliderComponent>(handle_)) {
        registry_->get<ColliderComponent>(handle_).box = box;
    } else {
        registry_->emplace<ColliderComponent>(handle_, box);
    }
}

TexturedModel *Entity::getModel() {
    return registry_->get<RenderComponent>(handle_).model;
}

float Entity::getTextureXOffset() {
    auto& rc = registry_->get<RenderComponent>(handle_);
    int row = rc.textureIndex % rc.model->getModelTexture()->getNumberOfRows();
    return static_cast<float>(row) / static_cast<float>(rc.model->getModelTexture()->getNumberOfRows());
}

float Entity::getTextureYOffset() {
    auto& rc = registry_->get<RenderComponent>(handle_);
    int column = rc.textureIndex % rc.model->getModelTexture()->getNumberOfRows();
    return static_cast<float>(column) / static_cast<float>(rc.model->getModelTexture()->getNumberOfRows());
}

glm::vec3 &Entity::getPosition() {
    // NOTE: Returns a direct reference into the registry's component storage.
    // Callers must not trigger registry structural changes (add/remove components
    // on this entity) while holding this reference, or it may become dangling.
    return registry_->get<TransformComponent>(handle_).position;
}

void Entity::setPosition(glm::vec3 translate) {
    registry_->get<TransformComponent>(handle_).position = translate;
}

void Entity::increasePosition(glm::vec3 translate) {
    registry_->get<TransformComponent>(handle_).position += translate;
}

glm::vec3 Entity::getRotation() {
    return registry_->get<TransformComponent>(handle_).rotation;
}

void Entity::rotate(glm::vec3 rotate) {
    registry_->get<TransformComponent>(handle_).rotation += rotate;
}

void Entity::setRotation(glm::vec3 rotate) {
    registry_->get<TransformComponent>(handle_).rotation = rotate;
}

void Entity::increaseScale(float scaleSize) {
    registry_->get<TransformComponent>(handle_).scale += scaleSize;
}

void Entity::setScale(float scaleSize) {
    registry_->get<TransformComponent>(handle_).scale = scaleSize;
}

float Entity::getScale() const {
    return std::as_const(*registry_).get<TransformComponent>(handle_).scale;
}

void Entity::setTransformation(glm::vec3 translate, glm::vec3 rotate, float scalar) {
    auto& tc = registry_->get<TransformComponent>(handle_);
    tc.position = translate;
    tc.rotation = rotate;
    tc.scale = scalar;
}

Material Entity::getMaterial() const {
    const auto& reg = std::as_const(*registry_);
    const auto& mc = reg.get<MaterialDataComponent>(handle_);
    if (!mc.activated) {
        return reg.get<RenderComponent>(handle_).model->getModelTexture()->getMaterial();
    }
    return mc.material;
}

void Entity::setMaterial(Material material) {
    registry_->get<MaterialDataComponent>(handle_).material = material;
}

bool Entity::hasMaterial() const {
    return std::as_const(*registry_).get<MaterialDataComponent>(handle_).activated;
}

void Entity::activateMaterial() {
    registry_->get<MaterialDataComponent>(handle_).activated = true;
}

void Entity::disableMaterial() {
    registry_->get<MaterialDataComponent>(handle_).activated = false;
}
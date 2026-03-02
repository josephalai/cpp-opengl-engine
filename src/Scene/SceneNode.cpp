// SceneNode.cpp

#include "SceneNode.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <functional>

SceneNode::SceneNode(const std::string& name) : name(name) {}

void SceneNode::addChild(std::unique_ptr<SceneNode> child) {
    children_.push_back(std::move(child));
}

std::unique_ptr<SceneNode> SceneNode::removeChild(const std::string& childName) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if ((*it)->name == childName) {
            auto removed = std::move(*it);
            children_.erase(it);
            return removed;
        }
    }
    return nullptr;
}

glm::mat4 SceneNode::computeLocalMatrix() const {
    glm::mat4 m(1.0f);
    m = glm::translate(m, localPosition);
    m = glm::rotate(m, glm::radians(localRotation.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(localRotation.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(localRotation.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, glm::vec3(localScale));
    return m;
}

void SceneNode::update(const glm::mat4& parentWorld) {
    worldTransform = parentWorld * computeLocalMatrix();
    for (auto& child : children_) {
        child->update(worldTransform);
    }
}

void SceneNode::traverse(const std::function<void(SceneNode*)>& visitor) {
    visitor(this);
    for (auto& child : children_) {
        child->traverse(visitor);
    }
}

SceneNode* SceneNode::findNode(const std::string& searchName) {
    if (name == searchName) return this;
    for (auto& child : children_) {
        SceneNode* found = child->findNode(searchName);
        if (found) return found;
    }
    return nullptr;
}

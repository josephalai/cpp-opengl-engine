// SceneGraph.cpp

#include "SceneGraph.h"

SceneGraph::SceneGraph() : root_(std::make_unique<SceneNode>("root")) {}

void SceneGraph::update() {
    root_->update(glm::mat4(1.0f));
}

void SceneGraph::traverse(const std::function<void(SceneNode*)>& visitor) {
    root_->traverse(visitor);
}

SceneNode* SceneGraph::findNode(const std::string& name) {
    return root_->findNode(name);
}

std::vector<Entity*> SceneGraph::collectEntities() {
    std::vector<Entity*> result;
    traverse([&result](SceneNode* node) {
        if (node->entity != nullptr) {
            result.push_back(node->entity);
        }
    });
    return result;
}

// SceneGraph.h — owns the root node, provides traversal and update.

#ifndef ENGINE_SCENEGRAPH_H
#define ENGINE_SCENEGRAPH_H

#include <memory>
#include <functional>
#include <string>

#include "SceneNode.h"

class SceneGraph {
public:
    SceneGraph();

    /// Access the root node directly.
    SceneNode* getRoot() { return root_.get(); }

    /// Propagate transforms top-down from the root.
    void update();

    /// Depth-first traversal starting from the root.
    void traverse(const std::function<void(SceneNode*)>& visitor);

    /// Find the first node (depth-first) with the given name. Returns nullptr if not found.
    SceneNode* findNode(const std::string& name);

    /// Convenience: collect all Entity* pointers from nodes that have one.
    std::vector<Entity*> collectEntities();

private:
    std::unique_ptr<SceneNode> root_;
};

#endif // ENGINE_SCENEGRAPH_H

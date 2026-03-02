// SceneNode.h — one node in the scene hierarchy.

#ifndef ENGINE_SCENENODE_H
#define ENGINE_SCENENODE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Entities/Entity.h"

/// A single node in the scene graph tree.
/// Each node has a *local* transform relative to its parent, and a *world*
/// transform that is recomputed by calling update().
class SceneNode {
public:
    std::string name;

    // -----------------------------------------------------------------------
    // Local transform (relative to parent)
    glm::vec3 localPosition  = glm::vec3(0.0f);
    glm::vec3 localRotation  = glm::vec3(0.0f); ///< Euler angles in degrees
    float     localScale     = 1.0f;

    // World transform (set by update())
    glm::mat4 worldTransform = glm::mat4(1.0f);

    // Optional renderable entity attached to this node (may be nullptr).
    Entity*   entity = nullptr;

    // -----------------------------------------------------------------------
    explicit SceneNode(const std::string& name = "");

    /// Add a child node; takes ownership.
    void addChild(std::unique_ptr<SceneNode> child);

    /// Remove the first child whose name matches; returns removed node or nullptr.
    std::unique_ptr<SceneNode> removeChild(const std::string& childName);

    /// Recompute worldTransform for this node and all descendants.
    /// @param parentWorld  world transform of the parent (identity for root).
    void update(const glm::mat4& parentWorld = glm::mat4(1.0f));

    /// Depth-first traversal: calls visitor(node) for every node (including self).
    void traverse(const std::function<void(SceneNode*)>& visitor);

    /// Find the first descendant (or self) whose name matches.
    SceneNode* findNode(const std::string& searchName);

    const std::vector<std::unique_ptr<SceneNode>>& getChildren() const { return children_; }

private:
    std::vector<std::unique_ptr<SceneNode>> children_;

    glm::mat4 computeLocalMatrix() const;
};

#endif // ENGINE_SCENENODE_H

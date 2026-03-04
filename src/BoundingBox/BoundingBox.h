//
// Created by Joseph Alai on 7/25/21.
//

#ifndef ENGINE_BOUNDINGBOX_H
#define ENGINE_BOUNDINGBOX_H
#include "RawBoundingBox.h"
#include "glm/glm.hpp"
#include "../Toolbox/Color.h"

/// Axis-Aligned Bounding Box in model/local space.
/// When valid == false, frustum culling treats the entity as always visible.
struct BoundingAABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    bool      valid = false;
};

class BoundingBox {
// BoundingBox: A RawBoundingBox + Color

private:
    RawBoundingBox *rawBoundingBox;
    Color color;
    BoundingAABB  aabb_;
public:

    /**
     * @brief BoundingBox stores the RawBoundingBox [the VaoId] (from loaded mesh details),
     *        the glm::vec3 color, so that it can be used to track clicks on the bounding
     *        boxes.
     *
     * @param rawBoundingBox
     * @param color
     */
    BoundingBox(RawBoundingBox *rawBoundingBox, glm::vec3 color) : rawBoundingBox(rawBoundingBox), color(color) {}

    RawBoundingBox *getRawBoundingBox() {
        return BoundingBox::rawBoundingBox;
    }

    /**
     * @brief Returns the Color (Unique ID) of the box
     *
     * @return
     */
    Color getBoxColor() {
        return BoundingBox::color;
    }

    /**
     * @brief Sets the Color (Unique ID) of the bounding box
     * @param color
     */
    void setBoxColor(const Color &color) {
        BoundingBox::color = color;
    }

    /// Returns the axis-aligned bounding box in model/local space.
    /// Check aabb.valid before using it for culling.
    BoundingAABB getAABB() const { return aabb_; }

    /// Set the local-space AABB (marks it as valid).
    void setAABB(const glm::vec3& minPt, const glm::vec3& maxPt) {
        aabb_.min   = minPt;
        aabb_.max   = maxPt;
        aabb_.valid = true;
    }
};
#endif //ENGINE_BOUNDINGBOX_H

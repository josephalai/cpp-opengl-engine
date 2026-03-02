//
// Created by Joseph Alai on 2/25/22.
//

#ifndef ENGINE_UICONSTRAINTS_H
#define ENGINE_UICONSTRAINTS_H


#include "glm/glm.hpp"
#include "UiConstraint.h"

class UiConstraints {

public:
    glm::vec2 position = glm::vec2(0.0f);

    UiConstraint *cx;

    UiConstraint *cy;

    // the parent's position modification
    glm::vec2 parentPosition = glm::vec2(0.0f);

    glm::vec2 size = glm::vec2(0.0f);

    bool scale = false;

public:
    UiConstraints();

    UiConstraints(float x, float y, float width, float height);

    UiConstraints(UiConstraint *x, UiConstraint *y, float width, float height);

    UiConstraints(glm::vec2 position, glm::vec2 size);

    glm::vec2 getConstraintPosition();

    /**
     * @brief Alias for getConstraintPosition() returning a mutable reference.
     */
    glm::vec2 &getPosition();

    /**
     * @brief Sets the relative position (of the parent)
     *
     * @param position
     */
    void setParentPosition(glm::vec2 position);

    /**
     * @brief Gets the relative position adjustment (position of the parent)
     *
     * @return
     */
    glm::vec2 getParentPosition();

    /**
     * @brief Gets the regular initial position, plus the relative position (modifications of the parent)
     *
     * @return
     */
    glm::vec2 getCalculatedRelativePosition();

    void setConstraintPosition(glm::vec2 position);

    void addConstraintPosition(glm::vec2 position);

    const glm::vec2 &getSize() const;

    void setSize(const glm::vec2 &size);

    float getX() const;

    void setX(float x);

    float getY() const;

    void setY(float y);

    float getWidth() const;

    void setWidth(float width);

    float getHeight() const;

    void setHeight(float height);
};


#endif //ENGINE_UICONSTRAINTS_H

//
// Created by Joseph Alai on 2/25/22.
//

#include "UiConstraints.h"
#include "../../Util/CommonHeader.h"
#include "../Constraints/Tools.h"

//UiConstraints::UiConstraints(float x, float y, float width, float height) : position(glm::vec2(x,y)), size(glm::vec2(width, height)) {}

UiConstraints::UiConstraints(float x, float y, float width, float height) : position(glm::vec2(x, y)), size(glm::vec2(width, height)) {}

UiConstraints::UiConstraints(glm::vec2 position, glm::vec2 size) : position(position), size(size) {}

float UiConstraints::getX() const {
    return UiConstraints::position.x;
}

void UiConstraints::setX(float x) {
    UiConstraints::position.x = x;
}

float UiConstraints::getY() const {
    return UiConstraints::position.y;
}

void UiConstraints::setY(float y) {
    UiConstraints::position.y = y;
}

float UiConstraints::getWidth() const {
    return UiConstraints::size.x;
}

void UiConstraints::setWidth(float width) {
    size.x = width;
}

float UiConstraints::getHeight() const {
    return UiConstraints::size.y;
}

void UiConstraints::setHeight(float height) {
    size.y = height;
}

UiConstraints::UiConstraints() {}

glm::vec2 UiConstraints::getConstraintPosition() {
    return position;
}

glm::vec2 &UiConstraints::getPosition() {
    return position;
}

void UiConstraints::setConstraintPosition(glm::vec2 position) {
    UiConstraints::position = position;
}

void UiConstraints::addConstraintPosition(glm::vec2 position) {
    this->position += position;
}

/**
 * @brief Gets the relative position plus the original position.
 *
 * @return
 */
glm::vec2 UiConstraints::getCalculatedRelativePosition() {
    const glm::vec2 &x = parentPosition + position;
    return x;
}

void UiConstraints::setParentPosition(glm::vec2 position) {
    this->parentPosition = position;
}

/**
 * @brief Gets the relativity of the parent.
 *
 * @return
 */
glm::vec2 UiConstraints::getParentPosition() {
    return parentPosition;
}

const glm::vec2 &UiConstraints::getSize() const {
    return size;
}

void UiConstraints::setSize(const glm::vec2 &size) {
    UiConstraints::size = size;
}

UiConstraints::UiConstraints(UiConstraint *x, UiConstraint *y, float width, float height) : cx(x), cy(y), size(glm::vec2(width, height)) {
    this->position = glm::vec2(x->getNormalizedValue(), y->getNormalizedValue());
}

//
// Created by Joseph Alai on 3/1/22.
//

#include "GuiRect.h"

/**
 * @brief GuiTexture stores the textureId, fontSize, and the vertex of the GUI
 * @param textureId
 * @param position
 * @param scale
 */
GuiRect::GuiRect(Color color, glm::vec2 &position, glm::vec2 &size, glm::vec2 &scale, float alpha) :
        color(color), position(position), scale(scale), size(size), alpha(alpha), GuiComponent(COLORED_BOX) {}


const glm::vec2 &GuiRect::getScale() {
    return scale;
}

const glm::vec3 GuiRect::getColor() const {
    return glm::vec3(color.getR(), color.getG(), color.getB());
}

void GuiRect::setColor(Color color) {
    GuiRect::color = color;
}

const glm::vec2 &GuiRect::getSize() const {
    return size;
}

void GuiRect::setSize(const glm::vec2 &size) {
    GuiRect::size = size;
}

float GuiRect::getAlpha() const {
    return alpha;
}

void GuiRect::setAlpha(float alpha) {
    GuiRect::alpha = alpha;
}

void GuiRect::setScale(const glm::vec2 &scale) {
    GuiRect::scale = scale;
}

//
// Created by Joseph Alai on 7/18/21.
//

#include "GuiTexture.h"

/**
 * @brief GuiTexture stores the textureId, fontSize, and the vertex of the GUI
 * @param textureId
 * @param position
 * @param scale
 */
GuiTexture::GuiTexture(unsigned int textureId, const glm::vec2 &position, const glm::vec2 &scale) :
        textureId(textureId), position(position), scale(scale), GuiComponent(IMAGE) {
    // Sync the inherited constraints with the NDC position so GuiRenderer::prepareInstance
    // reads the correct position for textures created outside the UiMaster hierarchy.
    // (UiMaster-managed textures have their constraints replaced by addChild anyway.)
    constraints->setConstraintPosition(position);
}

unsigned int GuiTexture::getTexture() {
    return textureId;
}

glm::vec2 &GuiTexture::getPosition() {
    return position;
}

const glm::vec2 &GuiTexture::getScale() {
    return scale;
}
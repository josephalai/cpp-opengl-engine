//
// Created by Joseph Alai on 7/23/21.
//

#include "GUIText.h"
#include "Rendering/TextMaster.h"

/**
 * @brief This inputs everything needed to render a Text GUI
 *
 * // todo: This automatically adds the text to TextMaster::load(this) in its' creation, which it should not be doing.
 *        So we should remove that last line, and make an explicit adding and removing of each text because we will
 *        need to do so.
 *
 * @param textString
 * @param fontSize
 * @param font
 * @param fontType
 * @param position
 * @param color
 * @param maxLineLength
 * @param centered
 */
GUIText::GUIText(const std::string &textString, float fontSize, FontModel *font, FontType *fontType, glm::vec2(position),
                 Color color, float maxLineLength = 1.0f, bool centered = false) : GuiComponent(TEXT) {
    this->textString = textString;
    this->font = font;
    this->fontType = fontType;
    this->position = position;
    this->fontSize = fontSize;
    this->color = color;
    this->lineMaxLength = maxLineLength;
    this->centerText = centered;
    // Sync the inherited constraints position so FontRenderer::prepareText
    // reads the correct NDC position for standalone (non-UiMaster) texts.
    // UiMaster-managed texts have constraints replaced by addChild anyway.
    constraints->setConstraintPosition(position);
    TextMaster::loadText(this);
}

const std::string &GUIText::getText() const {
    return this->textString;
}

FontModel *GUIText::getFont() {
    return this->font;
}

FontType *GUIText::getFontType() {
    return this->fontType;
}

float GUIText::getFontSize() const {
    return this->fontSize;
}

Color GUIText::getColor() {
    return this->color;
}

void GUIText::setColor(Color color) {
    this->color = color;
}

void GUIText::remove() {
    TextMaster::remove(this);
}

int GUIText::getNumberOfLines() const {
    return numberOfLines;
}

float GUIText::getMaxLineSize() const {
    return this->lineMaxLength;
}

void GUIText::setNumberOfLines(int number) {
    this->numberOfLines = number;
}

bool GUIText::isCentered() const {
    return centerText;
}
glm::vec2 &GUIText::getPosition() {
    return position;
}

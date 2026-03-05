//
// Created by Joseph Alai on 7/23/21.
//

#ifndef ENGINE_GUITEXT_H
#define ENGINE_GUITEXT_H

#include <iostream>
#include "glm/glm.hpp"
#include "FontMeshCreator/TextMeshData.h"
#include "FontMeshCreator/FontModel.h"
#include "../../Toolbox/Color.h"
#include "../GuiComponent.h"

class GUIText : public GuiComponent {
private:


    float lineMaxLength = 1.0f;

    FontModel *font;

    FontType *fontType;

    std::string textString;

    float fontSize;

    Color color = Color(0.0f);

    int numberOfLines;

    int textMeshVao;

    int vertexCount;

    bool centerText = false;
public:
    glm::vec2 position;

    /**
     * Creates a new textString, loads the textString's quads into a VAO, and adds the textString
     * to the screen.
     *
     * @param textString
     *            - the textString.
     * @param fontSize
     *            - the font fontSize of the textString, where a font fontSize of 1 is the
     *            default fontSize.
     * @param font
     *            - the font that this textString should use.
     * @param position
     *            - the position on the screen where the top left corner of the
     *            textString should be rendered. The top left corner of the screen is
     *            (0, 0) and the bottom right is (1, 1).
     * @param maxLineLength
     *            - basically the width of the virtual page in terms of screen
     *            width (1 is full screen width, 0.5 is half the width of the
     *            screen, etc.) Text cannot go off the edge of the page, so if
     *            the textString is longer than this length it will go onto the next
     *            line. When textString is centered it is centered into the middle of
     *            the line, based on this line length value.
     * @param centered
     *            - whether the textString should be centered or not.
     */
    GUIText(const std::string &textString, float fontSize, FontModel *font, FontType *fontType, glm::vec2(position),
            Color color, float maxLineLength, bool centered);

    FontType *getFontType();

    FontModel *getFont();

    float getFontSize() const;

    const std::string &getText() const;

    Color getColor();

    void setColor(Color color);

    void remove();

    /// Update the text string and color in place.
    /// Removes this text from TextMaster, updates fields, then re-registers.
    /// Use this instead of copy-assigning a new GUIText, which would leave a
    /// dangling temporary pointer inside TextMaster and crash on render.
    void updateText(const std::string& newText, Color newColor);

    int getNumberOfLines() const;

    int getMesh();

    void setMeshInfo(int vao, int verticesCount);

    int getVertexCount();

    float getMaxLineSize() const;

    void setNumberOfLines(int number);

    bool isCentered() const;

    glm::vec2 &getPosition();

//    void show() ;
//
//    bool isDisplayed() ;
//
//    void hide() ;
//
//    bool isClicked() ;
//
//    bool isMouseOver() ;
//
//    void setConstraints(UiConstraints constraints) ;
//
//    bool isMouseOverChild() ;

protected:

};


#endif //ENGINE_GUITEXT_H

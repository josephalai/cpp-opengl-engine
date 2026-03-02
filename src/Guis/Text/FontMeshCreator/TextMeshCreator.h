//
// Created by Joseph Alai on 7/24/21.
//

#ifndef ENGINE_TEXTMESHCREATOR_H
#define ENGINE_TEXTMESHCREATOR_H

#include <iostream>
#include "TextMeshData.h"
#include "Line.h"
#include "Character.h"
#include "Word.h"
#include "../GUIText.h"

class TextMeshCreator {
protected:
    constexpr static double kLineHeight = 0.03f;
    constexpr static int kSpaceAscii = 32;
    constexpr static int kNewLineAscii = 10;
public:
    static TextMeshData *createTextMesh(GUIText *text);
protected:

    static std::vector<Line > createStructure(GUIText *text);

private:
    static void completeStructure(std::vector<Line> *lines, Line *currentLine, Word *currentWord, GUIText *text);

    static TextMeshData *createQuadVertices(GUIText *text, std::vector<Line > *lines);
};


#endif //ENGINE_TEXTMESHCREATOR_H

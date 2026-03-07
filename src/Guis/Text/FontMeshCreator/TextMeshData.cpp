//
// Created by Joseph Alai on 7/22/21.
//

#include "TextMeshData.h"
#include "../../../Util/FileSystem.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/freetype.h>

#include <utility>

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include "GLFW/glfw3.h"
FontType::FontType(std::map<char, Character*> *characters)
        : characters(characters) {
}

Character *FontType::getCharacter(char c) {
    return characters->at(c);
}

FontType TextMeshData::loadFont(const std::string &location, const int size) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    }

    FT_Face face;
    if (FT_New_Face(ft, FileSystem::Font(location).c_str(), 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
    }

    FT_Set_Pixel_Sizes(face, 0, size);

    if (FT_Load_Char(face, 'X', FT_LOAD_RENDER)) {
        std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
    auto *fontCharacters = new std::map<char, Character*>;
    for (unsigned char c = 0; c < 128; c++) {
        // load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }
        // generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                static_cast<GLint>(face->glyph->bitmap.width),
                static_cast<GLint>(face->glyph->bitmap.rows),
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
        );
        // set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // now store character for later use
        auto *character = new Character(
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)
        );
        fontCharacters->insert(std::pair<char, Character*>(c, character));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return FontType(fontCharacters);
}

TextMeshData::TextMeshData(glm::vec2 startPos, std::vector<Line > lines, double lineHeight) {
    this->lines = lines;
    this->lineHeight = lineHeight;
    this->startPosition = startPos;
}

std::vector<Line > TextMeshData::getLines() const {
    return lines;
}

double TextMeshData::getLineHeight() const {
    return lineHeight;
}

glm::vec2 TextMeshData::getStartPos() const {
    return startPosition;
}

void TextMeshData::setStartPos(glm::vec2 position) {
    TextMeshData::startPosition = position;
}

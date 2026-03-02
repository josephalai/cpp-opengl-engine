//
// Created by Joseph Alai on 7/22/21.
//

#include "FontRenderer.h"
#include "../FontMeshCreator/TextMeshCreator.h"
#include "../../../OpenGLWrapper/OpenGLUtils.h"
#include "../../../Toolbox/Maths.h"
#include "../../../Util/CommonHeader.h"
#include "../../Constraints/Tools.h"

FontRenderer::FontRenderer() {
    shader = new FontShader();
    shader->start();
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(DisplayManager::Width()), 0.0f,
                                      static_cast<float>(DisplayManager::Height()));
    shader->loadProjectionMatrix(projection);
    shader->stop();
}

void FontRenderer::cleanUp() {
    shader->cleanUp();
}

void FontRenderer::render(std::map<FontType *, std::vector<GUIText *>> *texts) {
    prepare();
    std::map<FontType *, std::vector<GUIText *>>::iterator it;
    for (it = texts->begin(); it != texts->end(); it++) {
        for (GUIText *text : it->second) {
            prepareText(text);
            renderTextMesh(text);
        }
    }
    endRendering();
}

/**
 * @brief this takes in the text mesh data, and completes the process for rendering text.
 *        This function originally positioned the text in the VAO Data Array.
 *        But for consistency, and to be able to modify text later, position was extracted from the
 *        VAO Vertices Array itself. We added a transformation matrix variable (position, scale),
 *        to the Shader instead, so whatever transformations and positions need to applied to the
 *        text, it is done via the shader program.
 *        Simply put, instead of loading the x, y position to the VAO Array and directly
 *        hardcoding it to the Graphics card, we force every text position to start at (0, 0),
 *        but then it is transformed via a Shader to match the desired position formally hardcoded.
 *
 * @param text
 */
void FontRenderer::renderTextMesh(GUIText *text) {
    auto meshData = TextMeshCreator::createTextMesh(text);

    // if we want to hardcode the x and y positions, set x = text.getConstraintPosition.x, etc.
    float x = 0;
    float y = 0;

    shader->loadTextColor(text->getColor());

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(text->getFont()->getVaoId());

    for (int i = 0; i < meshData->getLines().size(); i++) {
        auto line = meshData->getLines()[i];
        x += line.getOffset().x;
        y -= static_cast<float>(meshData->getLineHeight() * i);
        for (auto word : line.getWords()) {
            for (auto ch : word.getCharacters()) {
                float xpos = x + static_cast<float>(ch.getBearing().x) * text->getFontSize();
                float ypos = y - (static_cast<float>(ch.getSize().y) - static_cast<float>(ch.getBearing().y)) *
                                 text->getFontSize();
                float w = static_cast<float>(ch.getSize().x) * text->getFontSize();
                float h = static_cast<float>(ch.getSize().y) * text->getFontSize();
                // update VBO for each character
                float vertices[6][4] = {
                        {xpos,     ypos + h, 0.0f, 0.0f},
                        {xpos,     ypos,     0.0f, 1.0f},
                        {xpos + w, ypos,     1.0f, 1.0f},

                        {xpos,     ypos + h, 0.0f, 0.0f},
                        {xpos + w, ypos,     1.0f, 1.0f},
                        {xpos + w, ypos + h, 1.0f, 0.0f}
                };
                // render glyph texture over quad
                glBindTexture(GL_TEXTURE_2D, ch.getTextureId());
                // update content of VBO memory
                glBindBuffer(GL_ARRAY_BUFFER, text->getFont()->getVboId());
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices),
                                vertices); // be sure to use glBufferSubData and not glBufferData

                glBindBuffer(GL_ARRAY_BUFFER, 0);

                // render quad
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                x += static_cast<float>(ch.getAdvance() >> 6) *
                     text->getFontSize(); // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
            }
        }

        x = 0;
        y = 0;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FontRenderer::prepare() {
    OpenGLUtils::cullBackFaces(true);
    OpenGLUtils::enableAlphaBlending();
    OpenGLUtils::enableDepthTest(false);
    shader->start();
}

/**
 * @brief Here we load the position and scale of the text to the shader. Defaults will place the text in (0,0).
 *
 * @param text
 */
void FontRenderer::prepareText(GUIText *text) {
    glm::vec2 v = text->getConstraints()->getCalculatedRelativePosition();
    v.x = ((v.x + 1) / 2) * static_cast<float>(DisplayManager::Width());
    v.y = ((v.y + 1) / 2) * static_cast<float>(DisplayManager::Height());
    glm::mat4 matrix = Maths::createTransformationMatrix(v, glm::vec2(1.0f));

    shader->loadTransformationMatrix(matrix);
}

void FontRenderer::endRendering() {
    shader->stop();

    OpenGLUtils::enableDepthTest(true);
    OpenGLUtils::disableBlending();
    OpenGLUtils::cullBackFaces(false);
}

/**
 * @brief Render a one-off... This was created for the UI Master and is not required for anything to work,
 *        The reason this is created is so we can add a render inside the UiMaster, and render different
 *        types of components in any order, instead of as vectors by type.
 *
 * @param text
 */
void FontRenderer::render(GUIText *text) {
    prepare();
    std::map<FontType *, std::vector<GUIText *>>::iterator it;
    prepareText(text);
    renderTextMesh(text);
    endRendering();
}

//
// Created by Joseph Alai on 7/18/21.
//

#include "GuiRenderer.h"
#include "../../../Toolbox/Maths.h"
#include "../../../OpenGLWrapper/OpenGLUtils.h"

/**
 * @brief GuiRenderer loads the vertex of a square (2d) into a Vao (RawModel).
 *        It also binds the textures, builds a shader, and renders the GUI to whatever
 *        vertex asked for.
 * @param loader
 */
GuiRenderer::GuiRenderer(Loader *loader) {
    std::vector<float> positions = {-1, 1, -1, -1, 1, 1, 1, -1};
    for (float & position : positions) {
        position = -position;
    }
    quad = loader->loadToVAO(positions, 2);
    shader = new GuiShader();
}

/**
 * @brief render (MAIN LOOP) starts the shader, binds the Vao,
 *        binds the texture, loads the transformations to the
 *        shader, draws the quad, and then unbinds it all, and stops
 *        the shader.
 * @param guis
 */
void GuiRenderer::render(std::vector<GuiTexture*> guis) {
    shader->start();
    glBindVertexArray(quad->getVaoId());
    glEnableVertexAttribArray(0);
    OpenGLUtils::enableAlphaBlending();
    OpenGLUtils::enableDepthTest(false);
    for (auto gui : guis) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gui->getTexture());
        prepareInstance(gui);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, quad->getVertexCount());
    }
    OpenGLUtils::enableDepthTest(true);
    OpenGLUtils::disableBlending();
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    shader->stop();
}

/**
 * @brief delete and detach Shader program.
 */
void GuiRenderer::cleanUp() {
    shader->cleanUp();
}

/**
 * @brief Loads any variables from the gui into the Shader program.
 *
 * @param gui
 */
void GuiRenderer::prepareInstance(GuiTexture *gui) {
    glm::mat4 matrix = Maths::createTransformationMatrix(gui->getConstraints()->getCalculatedRelativePosition(), gui->getScale());
    shader->loadTransformationMatrix(matrix);
}

/**
 * @bried Renders a single GuiTexture
 *
 * @param gui
 */
void GuiRenderer::render(GuiTexture *gui) {
    shader->start();
    glBindVertexArray(quad->getVaoId());
    glEnableVertexAttribArray(0);
    OpenGLUtils::enableAlphaBlending();
    OpenGLUtils::enableDepthTest(false);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gui->getTexture());
    prepareInstance(gui);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, quad->getVertexCount());
    OpenGLUtils::enableDepthTest(true);
    OpenGLUtils::disableBlending();
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    shader->stop();
}

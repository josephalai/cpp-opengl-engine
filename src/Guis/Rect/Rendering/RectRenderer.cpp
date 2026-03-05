//
// Created by Joseph Alai on 3/1/22.
//

#include "RectRenderer.h"
#include "../../../Toolbox/Maths.h"
#include "../../../OpenGLWrapper/OpenGLUtils.h"

/**
 * @brief RectRenderer loads the vertex of a square (2d) into a Vao (RawModel).
 *        It also binds the textures, builds a shader, and renders the GUI to whatever
 *        vertex asked for.
 * @param loader
 */
RectRenderer::RectRenderer(Loader *loader) {
    std::vector<float> positions = {-1, 1, -1, -1, 1, 1, 1, -1};
    for (float &position : positions) {
        position = -position;
    }
    quad = loader->loadToVAO(positions, 2);
    shader = new RectShader();
}

/**
 * @brief render (MAIN LOOP) starts the shader, binds the Vao,
 *        binds the texture, loads the transformations to the
 *        shader, draws the quad, and then unbinds it all, and stops
 *        the shader.
 * @param guis
 */
void RectRenderer::render(std::vector<GuiRect *> guis) {
    shader->start();
    glBindVertexArray(quad->getVaoId());
    glEnableVertexAttribArray(0);
    OpenGLUtils::enableAlphaBlending();
    OpenGLUtils::enableDepthTest(false);
    for (auto gui : guis) {
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
void RectRenderer::cleanUp() {
    shader->cleanUp();
}

/**
 * @brief Loads any variables from the gui into the Shader program.
 *
 * @param gui
 */
void RectRenderer::prepareInstance(GuiRect *gui) {
    glm::mat4 matrix = Maths::createTransformationMatrix(gui->getConstraints()->getCalculatedRelativePosition(), gui->getScale());
    shader->loadTransformationMatrix(matrix);
    shader->loadColor(gui->getColor());
    shader->loadAlpha(gui->getAlpha());
}



/**
 * @brief render (MAIN LOOP) starts the shader, binds the Vao,
 *        binds the texture, loads the transformations to the
 *        shader, draws the quad, and then unbinds it all, and stops
 *        the shader.
 * @param guis
 */
void RectRenderer::render(GuiRect * gui) {
    shader->start();
    glBindVertexArray(quad->getVaoId());
    glEnableVertexAttribArray(0);
    OpenGLUtils::enableAlphaBlending();
    OpenGLUtils::enableDepthTest(false);
    prepareInstance(gui);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, quad->getVertexCount());
    OpenGLUtils::enableDepthTest(true);
    OpenGLUtils::disableBlending();
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    shader->stop();
}

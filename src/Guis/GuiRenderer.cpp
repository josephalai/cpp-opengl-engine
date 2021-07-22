//
// Created by Joseph Alai on 7/18/21.
//

#include "GuiRenderer.h"
#include "../Toolbox/Maths.h"

/**
 * @brief GuiRenderer loads the position of a square (2d) into a Vao (RawModel).
 *        It also binds the textures, builds a shader, and renders the GUI to whatever
 *        position asked for.
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    for (auto gui : guis) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gui->getTexture());
        glm::mat4 matrix = Maths::createTransformationMatrix(gui->getPosition(), gui->getScale());
        shader->loadTransformationMatrix(matrix);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, quad->getVertexCount());
    }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    shader->stop();
}

void GuiRenderer::cleanUp() {
    shader->cleanUp();
}

//
// Created by Joseph Alai on 7/18/21.
//

#include "GuiRenderer.h"
#include "../Toolbox/Maths.h"

GuiRenderer::GuiRenderer(Loader *loader) {
    std::vector<float> positions = {-1, 1, -1, -1, 1, 1, 1, -1};
    for (float & position : positions) {
        position = -position;
    }
    quad = loader->loadToVAO(positions);
    shader = new GuiShader();
}

void GuiRenderer::render(std::vector<GuiTexture*> guis) {
    shader->start();
    glBindVertexArray(quad->getVaoID());
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

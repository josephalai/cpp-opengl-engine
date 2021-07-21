//
// Created by Joseph Alai on 7/20/21.
//

#include "SkyboxRenderer.h"

SkyboxRenderer::SkyboxRenderer(Loader *loader, glm::mat4 projectionMatrix) {
    cube = loader->loadToVAO(VERTICES, 3);
    texture = loader->loadCubeMap(TEXTURE_FILES);
    shader = new SkyboxShader();
    shader->start();
    shader->loadProjectionMatrix(projectionMatrix);
    shader->stop();
}

void SkyboxRenderer::render(Camera *camera) {
    glDepthMask(GL_FALSE);
    shader->start();
    glm::mat4 view = glm::mat4(glm::mat3(camera->GetViewMatrix()));
    shader->loadViewMatrix(view);
    glBindVertexArray(cube->getVaoID());
    glEnableVertexAttribArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glDrawArrays(GL_TRIANGLES, 0, cube->getVertexCount());
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    shader->stop();
    glDepthMask(GL_TRUE);
}

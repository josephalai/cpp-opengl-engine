// src/RenderEngine/InstancedModel.cpp

#include "InstancedModel.h"

InstancedModel::InstancedModel(GLuint vao, int idxCount, GLuint tex)
    : vaoID(vao), indexCount(idxCount), textureID(tex) {}

void InstancedModel::setupInstanceVBO() {
    glBindVertexArray(vaoID);

    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    // Reserve space for up to 1024 instances (will reallocate as needed)
    glBufferData(GL_ARRAY_BUFFER, 1024 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

    // A mat4 occupies 4 consecutive vec4 attribute locations (3, 4, 5, 6)
    for (int col = 0; col < 4; ++col) {
        GLuint loc = 3u + static_cast<GLuint>(col);
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(sizeof(glm::mat4)),
                              reinterpret_cast<void*>(col * sizeof(glm::vec4)));
        glVertexAttribDivisor(loc, 1);  // advance once per instance
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void InstancedModel::setInstances(const std::vector<glm::mat4>& transforms) {
    instances = transforms;
}

// src/Animation/AnimatedModel.cpp

#include "AnimatedModel.h"
#include <iostream>

int AnimatedModel::getClipIndex(const std::string& name) const {
    for (int i = 0; i < static_cast<int>(clips.size()); ++i) {
        if (clips[i].name == name) return i;
    }
    return -1;
}

void AnimatedModel::setupMeshes() {
    for (auto& mesh : meshes) {
        setupMesh(mesh);
    }
}

void AnimatedModel::setupMesh(AnimatedMesh& mesh) {
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(AnimatedVertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(unsigned int)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    // location 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
                          reinterpret_cast<void*>(offsetof(AnimatedVertex, position)));
    // location 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
                          reinterpret_cast<void*>(offsetof(AnimatedVertex, normal)));
    // location 2: texCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
                          reinterpret_cast<void*>(offsetof(AnimatedVertex, texCoords)));
    // location 3: boneIDs (ivec4)
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(AnimatedVertex),
                           reinterpret_cast<void*>(offsetof(AnimatedVertex, boneIDs)));
    // location 4: boneWeights
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(AnimatedVertex),
                          reinterpret_cast<void*>(offsetof(AnimatedVertex, boneWeights)));

    glBindVertexArray(0);
}

void AnimatedModel::cleanUp() {
    for (auto& mesh : meshes) {
        if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
        if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
        if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    }
}

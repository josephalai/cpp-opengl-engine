// src/Animation/BoneBuffer.cpp

#include "BoneBuffer.h"
#include <algorithm>
#include <iostream>

void BoneBuffer::init(int maxBones) {
    if (ubo_ != 0) return;  // already initialised

    maxBones_ = maxBones;

    glGenBuffers(1, &ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    // Allocate maxBones mat4 slots, content will be filled by upload()
    glBufferData(GL_UNIFORM_BUFFER,
                 static_cast<GLsizeiptr>(maxBones * sizeof(glm::mat4)),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void BoneBuffer::upload(const std::vector<glm::mat4>& matrices) {
    if (ubo_ == 0) {
        std::cerr << "[BoneBuffer] upload() called before init()\n";
        return;
    }

    int count = std::min(static_cast<int>(matrices.size()), maxBones_);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(count * sizeof(glm::mat4)),
                    matrices.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void BoneBuffer::bind(GLuint bindingPoint) const {
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo_);
}

void BoneBuffer::cleanup() {
    if (ubo_ != 0) {
        glDeleteBuffers(1, &ubo_);
        ubo_      = 0;
        maxBones_ = 0;
    }
}

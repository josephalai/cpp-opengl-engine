//
// PhysicsDebugDrawer.cpp
//

#include "PhysicsDebugDrawer.h"
#include "DebugLineShader.h"

#include <iostream>

PhysicsDebugDrawer::PhysicsDebugDrawer() = default;

PhysicsDebugDrawer::~PhysicsDebugDrawer() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    delete shader_;
}

void PhysicsDebugDrawer::init() {
    shader_ = new DebugLineShader();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // Preallocate 64k vertices
    glBufferData(GL_ARRAY_BUFFER, sizeof(LineVertex) * 65536, nullptr, GL_DYNAMIC_DRAW);

    // position — attribute 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(0));
    // color — attribute 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
}

void PhysicsDebugDrawer::drawLine(const btVector3& from, const btVector3& to,
                                   const btVector3& color) {
    vertices_.push_back({from.x(), from.y(), from.z(), color.x(), color.y(), color.z()});
    vertices_.push_back({to.x(),   to.y(),   to.z(),   color.x(), color.y(), color.z()});
}

void PhysicsDebugDrawer::drawContactPoint(const btVector3& pointOnB,
                                           const btVector3& normalOnB,
                                           btScalar distance, int /*lifeTime*/,
                                           const btVector3& color) {
    btVector3 tip = pointOnB + normalOnB * distance;
    drawLine(pointOnB, tip, color);
}

void PhysicsDebugDrawer::reportErrorWarning(const char* warningString) {
    std::cerr << "[Bullet] " << warningString << std::endl;
}

void PhysicsDebugDrawer::draw3dText(const btVector3& /*location*/,
                                     const char* /*textString*/) {
    // Text rendering not implemented for debug drawer
}

void PhysicsDebugDrawer::flushLines(const glm::mat4& view,
                                     const glm::mat4& projection) {
    if (vertices_.empty() || !shader_) return;

    shader_->start();
    shader_->loadProjectionMatrix(projection);
    shader_->loadViewMatrix(view);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    size_t byteSize = vertices_.size() * sizeof(LineVertex);
    // Grow buffer if necessary
    static size_t vboCapacity = 65536 * sizeof(LineVertex);
    if (byteSize > vboCapacity) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(byteSize),
                     vertices_.data(), GL_DYNAMIC_DRAW);
        vboCapacity = byteSize;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(byteSize),
                        vertices_.data());
    }

    // Disable depth test so debug wireframes are always visible, even when occluded
    // by scene geometry. Save and restore the previous state rather than
    // unconditionally re-enabling, in case the caller had depth testing off.
    GLboolean depthTestWasEnabled = GL_FALSE;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestWasEnabled);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices_.size()));
    if (depthTestWasEnabled) glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    shader_->stop();

    vertices_.clear();
}

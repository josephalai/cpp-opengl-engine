//
// PhysicsDebugDrawer.h — implements btIDebugDraw to render Bullet debug lines
// via a simple OpenGL VBO + DebugLineShader.
//

#ifndef ENGINE_PHYSICSDEBUGDRAWER_H
#define ENGINE_PHYSICSDEBUGDRAWER_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include <LinearMath/btIDebugDraw.h>
#include <glm/glm.hpp>

#include <vector>

class DebugLineShader;

/// btIDebugDraw implementation that accumulates line geometry into a VBO and
/// renders it with DebugLineShader.
class PhysicsDebugDrawer : public btIDebugDraw {
public:
    PhysicsDebugDrawer();
    ~PhysicsDebugDrawer() override;

    /// One-time GPU resource setup — call after GL context is active.
    void init();

    /// Upload accumulated vertices and draw them.
    void flushLines(const glm::mat4& view, const glm::mat4& projection);

    // --- btIDebugDraw interface ---
    void drawLine(const btVector3& from, const btVector3& to,
                  const btVector3& color) override;

    void drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB,
                          btScalar distance, int lifeTime,
                          const btVector3& color) override;

    void reportErrorWarning(const char* warningString) override;

    void draw3dText(const btVector3& location, const char* textString) override;

    void setDebugMode(int mode) override { debugMode_ = mode; }
    int  getDebugMode() const   override { return debugMode_; }

private:
    struct LineVertex {
        float x, y, z;      ///< position
        float r, g, b;      ///< colour
    };

    std::vector<LineVertex> vertices_;

    GLuint vao_    = 0;
    GLuint vbo_    = 0;

    DebugLineShader* shader_ = nullptr;

    int debugMode_ = DBG_DrawWireframe | DBG_DrawAabb;
};

#endif // ENGINE_PHYSICSDEBUGDRAWER_H

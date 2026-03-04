//
// DebugLineShader.h — minimal GLSL shader for rendering physics debug lines.
//

#ifndef ENGINE_DEBUGLINESHADER_H
#define ENGINE_DEBUGLINESHADER_H

#include "../Shaders/ShaderProgram.h"
#include "glm/glm.hpp"

class DebugLineShader : public ShaderProgram {
private:
    constexpr static const char* VertexPath   = "/src/Physics/Shaders/DebugLineVertex.glsl";
    constexpr static const char* FragmentPath = "/src/Physics/Shaders/DebugLineFragment.glsl";

    const std::string positionAttr = "position";
    const std::string colorAttr    = "color";

    GLint location_projectionMatrix;
    GLint location_viewMatrix;

public:
    DebugLineShader();

    void bindAttributes() override;
    void getAllUniformLocations() override;

    void loadProjectionMatrix(const glm::mat4& m);
    void loadViewMatrix(const glm::mat4& m);
};

#endif // ENGINE_DEBUGLINESHADER_H

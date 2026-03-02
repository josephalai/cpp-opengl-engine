//
// Created by Joseph Alai on 7/22/21.
//

#ifndef ENGINE_FONTSHADER_H
#define ENGINE_FONTSHADER_H


#include "../../../Shaders/ShaderProgram.h"
#include "../../../RenderEngine/DisplayManager.h"
#include "../../../Toolbox/Color.h"
#include <glm/gtc/matrix_transform.hpp>

class FontShader : public ShaderProgram {
private:

    constexpr static const char *VertexPath = "/src/Guis/Text/Rendering/Shaders/VertexShader.glsl";
    constexpr static const char *FragmentPath = "/src/Guis/Text/Rendering/Shaders/FragmentShader.glsl";
    // attribute names
    const std::string vertex = "vertex";

    // uniform names
    const std::string projectionMatrix = "projection";
    const std::string textColor = "textColor";
    const std::string transformationMatrix = "transformationMatrix";

    GLint location_projectionMatrix;
    GLint location_textColor;
    GLint location_transformationMatrix;
public:
    FontShader();

    void bindAttributes() override;

    void loadProjectionMatrix(glm::mat4 matrix = glm::ortho(0.0f, static_cast<float>(DisplayManager::Width()), 0.0f,
                                                            static_cast<float>(DisplayManager::Height())));

    void loadTransformationMatrix(glm::mat4 matrix = glm::mat4(1.0f));

    void loadTextColor(Color textColor = Color(0.0f));

protected:
    void getAllUniformLocations() override;
};


#endif //ENGINE_FONTSHADER_H

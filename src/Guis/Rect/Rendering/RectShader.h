//
// Created by Joseph Alai on 3/1/22.
//

#ifndef ENGINE_RECTSHADER_H
#define ENGINE_RECTSHADER_H

#include "../../../Entities/CameraInput.h"
#include "../../../Textures/ModelTexture.h"
#include "../../../Shaders/ShaderProgram.h"
#include "../../../Entities/Light.h"

class RectShader : public ShaderProgram {

private:
    constexpr static const char *VertexPath = "/src/Guis/Rect/Rendering/Shaders/VertexShader.glsl";
    constexpr static const char *FragmentPath = "/src/Guis/Rect/Rendering/Shaders/FragmentShader.glsl";
    // attribute names
    const std::string position = "position";

    // uniform names
    const std::string transformationMatrix = "transformationMatrix";

    const std::string color = "color";

    const std::string alpha = "alpha";

    GLint location_transformationMatrix;

    GLint location_color;

    GLint location_alpha;
public:

    RectShader();

    void bindAttributes() override;

    void loadTransformationMatrix(glm::mat4 matrix = glm::mat4(1.0f));

    void loadColor(glm::vec3 color = glm::vec3(1.0f));

    void loadAlpha(float alpha = 1.0f);

protected:
    void getAllUniformLocations() override;

};


#endif //ENGINE_RECTSHADER_H

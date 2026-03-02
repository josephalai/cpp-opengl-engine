//
// Created by Joseph Alai on 7/18/21.
//

#ifndef ENGINE_GUISHADER_H
#define ENGINE_GUISHADER_H


#include "../../../Entities/CameraInput.h"
#include "../../../Textures/ModelTexture.h"
#include "../../../Shaders/ShaderProgram.h"
#include "../../../Entities/Light.h"


class GuiShader : public ShaderProgram {

private:
    constexpr static const char *VertexPath = "/src/Guis/Texture/Rendering/Shaders/VertexShader.glsl";
    constexpr static const char *FragmentPath = "/src/Guis/Texture/Rendering/Shaders/FragmentShader.glsl";
    // attribute names
    const std::string position = "position";

    // uniform names
    const std::string transformationMatrix = "transformationMatrix";


    GLint location_transformationMatrix;
public:

    GuiShader();

    void bindAttributes() override ;

    void loadTransformationMatrix(glm::mat4 matrix = glm::mat4(1.0f)) ;

protected:
    void getAllUniformLocations() override;

};


#endif //ENGINE_GUISHADER_H

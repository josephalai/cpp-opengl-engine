#ifndef SHADER_H
#define SHADER_H
#define GL_SILENCE_DEPRECATION

#include "../Util/FileSystem.h"
#define GLFW_INCLUDE_GLCOREARB
#include "GLFW/glfw3.h"
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
typedef enum {
    VertexShader,
    FragmentShader,
    GeometryShader
} ShaderType;

class Shader {
public:
    unsigned int shaderId;

    Shader(std::string file, ShaderType type);

    void loadShader(std::string path, ShaderType type);

private:
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(GLuint shader, std::string type);

    const std::string &fileName;
};

class ShaderProgram {
public:
    unsigned int programID;

    // constructor generates the shader on the fly
    // ------------------------------------------------------------------------
    ShaderProgram(const char *vertexPath, const char *fragmentPath, const char *geometryPath = nullptr);


    void initialize();

    Shader *getVertexShader() {
        return this->vertexShader;
    }

    Shader *getFragmentShader() {
        return this->fragmentShader;
    }

    Shader *getGeometryShader() {
        return this->geometryShader;
    }

    void bindAttribute(int attribute, std::string variableName);

    GLuint getAttribute(std::string variableName);

    // textureActivated the shader
    // ------------------------------------------------------------------------
    void start();

    void stop();

    /**
     * @brief Delete and detach Shader program.
     */
    void cleanUp();

    // utility uniform functions with a string.
    // ------------------------------------------------------------------------
    void setBool(const std::string &name, bool value) const {
        glUniform1i(glGetUniformLocation(programID, name.c_str()), static_cast<int>(value));
    }

    // ------------------------------------------------------------------------
    void setInt(const std::string &name, int value) const {
        glUniform1i(glGetUniformLocation(programID, name.c_str()), value);
    }

    // ------------------------------------------------------------------------
    void setFloat(const std::string &name, float value) const {
        glUniform1f(glGetUniformLocation(programID, name.c_str()), value);
    }

    // ------------------------------------------------------------------------
    void setVec2(const std::string &name, const glm::vec2 &value) const {
        glUniform2fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
    }

    void setVec2(const std::string &name, float x, float y) const {
        glUniform2f(glGetUniformLocation(programID, name.c_str()), x, y);
    }

    // ------------------------------------------------------------------------
    void setVec3(const std::string &name, const glm::vec3 &value) const {
        glUniform3fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
    }

    void setVec3(const std::string &name, float x, float y, float z) const {
        glUniform3f(glGetUniformLocation(programID, name.c_str()), x, y, z);
    }

    // ------------------------------------------------------------------------
    void setVec4(const std::string &name, const glm::vec4 &value) const {
        glUniform4fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
    }

    void setVec4(const std::string &name, float x, float y, float z, float w) {
        glUniform4f(glGetUniformLocation(programID, name.c_str()), x, y, z, w);
    }

    // ------------------------------------------------------------------------
    void setMat2(const std::string &name, const glm::mat2 &mat) const {
        glUniformMatrix2fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

    // ------------------------------------------------------------------------
    void setMat3(const std::string &name, const glm::mat3 &mat) const {
        glUniformMatrix3fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

    // ------------------------------------------------------------------------
    void setMat4(const std::string &name, const glm::mat4 &mat) const {
        glUniformMatrix4fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

    // this utilizes the location instead of the string name. it makes it a lot cheaper.

    // utility uniform functions
    // ------------------------------------------------------------------------
    void setBool(GLuint location, bool value) const {
        glUniform1i(location, static_cast<int>(value));
    }

    // ------------------------------------------------------------------------
    void setInt(GLuint location, int value) const {
        glUniform1i(location, value);
    }

    // ------------------------------------------------------------------------
    void setFloat(GLuint location, float value) const {
        glUniform1f(location, value);
    }

    // ------------------------------------------------------------------------
    void setVec2(GLuint location, const glm::vec2 &value) const {
        glUniform2fv(location, 1, &value[0]);
    }

    void setVec2(GLuint location, float x, float y) const {
        glUniform2f(location, x, y);
    }

    // ------------------------------------------------------------------------
    void setVec3(GLuint location, const glm::vec3 &value) const {
        glUniform3fv(location, 1, &value[0]);
    }

    void setVec3(GLuint location, float x, float y, float z) const {
        glUniform3f(location, x, y, z);
    }

    // ------------------------------------------------------------------------
    void setVec4(GLuint location, const glm::vec4 &value) const {
        glUniform4fv(location, 1, &value[0]);
    }

    void setVec4(GLuint location, float x, float y, float z, float w) {
        glUniform4f(location, x, y, z, w);
    }

    // ------------------------------------------------------------------------
    void setMat2(GLuint location, const glm::mat2 &mat) const {
        glUniformMatrix2fv(location, 1, GL_FALSE, &mat[0][0]);
    }

    // ------------------------------------------------------------------------
    void setMat3(GLuint location, const glm::mat3 &mat) const {
        glUniformMatrix3fv(location, 1, GL_FALSE, &mat[0][0]);
    }

    // ------------------------------------------------------------------------
    void setMat4(GLuint location, const glm::mat4 &mat) const {
        glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
    }

protected:

    int getUniformLocation(const std::string& uniformName) const {
        return glGetUniformLocation(this->programID, uniformName.c_str());
    }
    // implementation inside child class
    virtual void bindAttributes() = 0;

    virtual void getAllUniformLocations() = 0;

private:

    Shader *vertexShader;
    Shader *fragmentShader;
    Shader *geometryShader;

    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(GLuint shader, std::string type);
};

#endif

//
// Created by Joseph Alai on 7/6/21.
//

#include "ShaderProgram.h"


Shader::Shader(std::string file, ShaderType type) : fileName(std::move(file)){
    loadShader(file, type);
}

void Shader::loadShader(std::string path, ShaderType type) {
    auto bytes = FileSystem::readAllBytes(path);
    std::string code(bytes.begin(), bytes.end());
    if (code.empty()) {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
    }
    const char *vShaderCode = code.c_str();
    GLuint shaderBit;
    std::string debugType;
    switch (type) {
        case VertexShader:
            shaderBit = GL_VERTEX_SHADER;
            debugType = "VERTEX";
            break;
        case FragmentShader:
            shaderBit = GL_FRAGMENT_SHADER;
            debugType = "FRAGMENT";
            break;
        case GeometryShader:
            shaderBit = GL_GEOMETRY_SHADER;
            debugType = "GEOMETRY";
            break;
        default:
            break;
    }
    shaderId = glCreateShader(shaderBit);
    glShaderSource(shaderId, 1, &vShaderCode, NULL);
    glCompileShader(shaderId);
    checkCompileErrors(shaderId, debugType);
    printf("finished compiling process: %s\n", path.c_str());
}

// utility function for checking shader compilation/linking errors.
// ------------------------------------------------------------------------
void Shader::checkCompileErrors(GLuint shader, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog
                      << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog
                      << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    if (!success) {
        std::cout << "ERROR ON " + fileName << std::endl;
    }
}




ShaderProgram::ShaderProgram(const char *vertexPath, const char *fragmentPath, const char *geometryPath) {
    std::string pathVertex = FileSystem::Path(vertexPath);
    std::string pathFragment = FileSystem::Path(fragmentPath);

    vertexShader = new Shader(pathVertex, VertexShader);
    fragmentShader = new Shader(pathFragment, FragmentShader);
    geometryShader = nullptr;
    if (geometryPath != nullptr) {
        geometryShader = new Shader(geometryPath, GeometryShader);
    }
}


void ShaderProgram::initialize() {
    // shader Program
    programID = glCreateProgram();

    bindAttributes();
    glAttachShader(programID, vertexShader->shaderId);
    glAttachShader(programID, fragmentShader->shaderId);
    if (this->geometryShader != nullptr)
        glAttachShader(programID, geometryShader->shaderId);

    glLinkProgram(programID);
    checkCompileErrors(programID, "PROGRAM");

    getAllUniformLocations();
}

void ShaderProgram::bindAttribute(int attribute, std::string variableName) {
    glBindAttribLocation(this->programID, attribute, variableName.c_str());
}

GLuint ShaderProgram::getAttribute(std::string variableName) {
    return glGetAttribLocation(programID, variableName.c_str());
}

// textureActivated the shader
// ------------------------------------------------------------------------
void ShaderProgram::start() {
    glUseProgram(programID);
}

void ShaderProgram::stop() {
    glUseProgram(0);
}

void ShaderProgram::cleanUp() {
    stop();
    // delete the shaders as they're linked into our program now and no longer necessery
    glDeleteShader(vertexShader->shaderId);
    glDeleteShader(fragmentShader->shaderId);
    if (geometryShader != nullptr)
        glDeleteShader(geometryShader->shaderId);
    glDetachShader(programID, vertexShader->shaderId);
    glDetachShader(programID, fragmentShader->shaderId);
    if (geometryShader != nullptr)
        glDetachShader(programID, geometryShader->shaderId);
    glDeleteProgram(programID);
}

void ShaderProgram::checkCompileErrors(GLuint shader, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog
                      << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog
                      << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}
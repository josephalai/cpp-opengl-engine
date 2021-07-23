//
// Created by Joseph Alai on 6/30/21.
//

#ifndef ENGINE_LOADER_H
#define ENGINE_LOADER_H

#include <vector>
#include "../Models/RawModel.h"
#include "../Textures/TextureLoader.h"
#include "ModelData.h"
#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include "../FontMeshCreator/FontModel.h"

class Loader {
public:
    RawModel *loadToVAO(std::vector<GLfloat> positions, std::vector<GLfloat> textureCoords, std::vector<GLfloat> normals, std::vector<GLint> indices);

    /**
     * @brief loadToVAO inputs all the positions, indices, textureCoords, and normals inside an object
     * @param data
     * @return
     */
    RawModel *loadToVAO(ModelData *data);

    // for GUI rendering: we only need a 2d square with things rendered on it.
    RawModel *loadToVAO(std::vector<float> positions, int dimensions);

    FontModel *loadFontVAO(int vertices, int size, int bitSize);

    TextureLoader *loadTexture(std::string fileName);

    unsigned int loadCubeMap(std::vector<std::string> faces);

    void cleanUp();

private:
    std::vector<GLuint> vaos;
    std::vector<GLuint> vbos;
    std::vector<GLuint> textures;

    GLuint createVAO();

    void storeDataInAttributeList(GLuint attributeNumber, int coordinateSize, std::vector<GLfloat> positions);

    void bindIndicesBuffer(std::vector<GLint> indices);

    void initDynamicAttributeList(GLuint attributeNumber, GLuint vboNumber, int vertices, int size, int bitSize);

    void unbindVAO();

    GLuint createVBO();
};

#endif //ENGINE_LOADER_H

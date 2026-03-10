//
// Created by Joseph Alai on 7/6/21.
//

#include "Loader.h"
#include "../Util/FileSystem.h"
#include "../Libraries/images/stb_image.h"

#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <utility>

/**
 * @name loadToVAO
 * @brief inputs positions, textureCoords, normals, and kBboxIndices,
 *        creates a Vao, binds an kBboxIndices buffer, stores the data
 *        in the Data Attribute Lists, unbinds, and creates and
 *        returns a RawModel (for terrains and entities mainly)
 * @param positions
 * @param textureCoords
 * @param normals
 * @param indices
 * @return
 */
RawModel *
Loader::loadToVAO(std::vector<GLfloat> positions, std::vector<GLfloat> textureCoords, std::vector<GLfloat> normals,
                  const std::vector<GLint> &indices) {
    unsigned int vaoId = createVAO();
    this->bindIndicesBuffer(indices);
    this->storeDataInAttributeList(0, 3, std::move(positions));
    this->storeDataInAttributeList(1, 2, std::move(textureCoords));
    this->storeDataInAttributeList(2, 3, std::move(normals));
    this->unbindVAO();
    return new RawModel(vaoId, indices.size());
}

/**
 * @name loadToVAO
 * @brief inputs a ModelData *data object, which includes all the vectors of vertex,
 *        normals, texCoords, and kBboxIndices to store as VAO and return a RawModel. Used for
 *        meshes and terrains
 * @param data
 * @return
 */
RawModel *Loader::loadToVAO(MeshData &data) {
    return loadToVAO(data.getVertices(), data.getTextureCoords(), data.getNormals(), data.getIndices());
}

/**
 * @brief inputs positions and dimensions, creates VAO, return RawModel
 *        this is mainly for 2D models (gui, sky boxes).
 * @param positions
 * @param dimensions
 * @return
 */
RawModel *Loader::loadToVAO(const std::vector<GLfloat> &positions, int dimensions) {
    GLuint vaoId = createVAO();
    this->storeDataInAttributeList(0, dimensions, positions);
    this->unbindVAO();
    return new RawModel(vaoId, positions.size() / dimensions);
}

RawBoundingBox *Loader::loadToVAO(BoxData box) {
    GLuint vaoId = createVAO();
    glm::vec3 vBoxVertices[] =
            {
                    // Front wall of bounding box
                    box.vLowerLeftFront,
                    glm::vec3(box.vUpperRightBack.x, box.vLowerLeftFront.y, box.vLowerLeftFront.z),
                    glm::vec3(box.vLowerLeftFront.x, box.vUpperRightBack.y, box.vLowerLeftFront.z),
                    glm::vec3(box.vUpperRightBack.x, box.vUpperRightBack.y, box.vLowerLeftFront.z),

                    // Back wall of bounding box
                    glm::vec3(box.vLowerLeftFront.x, box.vLowerLeftFront.y, box.vUpperRightBack.z),
                    glm::vec3(box.vUpperRightBack.x, box.vLowerLeftFront.y, box.vUpperRightBack.z),
                    glm::vec3(box.vLowerLeftFront.x, box.vUpperRightBack.y, box.vUpperRightBack.z),
                    box.vUpperRightBack
            };

    this->bindIndicesBuffer(box.indices);

    std::vector<float> positions;
    for (glm::vec3 vBox: vBoxVertices) {
        positions.push_back(vBox.x);
        positions.push_back(vBox.y);
        positions.push_back(vBox.z);
    }
    this->storeDataInAttributeList(0, 3, positions);
    unbindVAO();
    return new RawBoundingBox(vaoId, box.indices.size());
}

RawBoundingBox *Loader::loadToVAO(BoundingBoxData box) {
    GLuint vaoId = createVAO();

    this->bindIndicesBuffer(box.getIndices());
    this->storeDataInAttributeList(0, 3, std::move(box.getVertices()));

    unbindVAO();
    return new RawBoundingBox(vaoId, box.getIndices().size(), false);
}

FontModel *Loader::loadFontVAO() {
    GLuint vaoId = createVAO();
    GLuint vboId = createVBO();
    this->initDynamicAttributeList(vboId);
    this->unbindVAO();
    return new FontModel(vaoId, vboId, FontModel::kVertexCount);
}

GLuint Loader::createVBO() {
    GLuint vboId;
    glGenBuffers(1, &vboId);
    vbos.push_back(vboId);
    return vboId;
}

/**
 * Loads a texture and returns it's id.
 * @param fileName
 * @return
 */
TextureLoader *Loader::loadTexture(std::string fileName) {
    auto *tex = new TextureLoader(FileSystem::Texture(std::move(fileName)), PNG);
    textures.push_back(tex->getId());
    return tex;
}

void Loader::cleanUp() {
    for (unsigned int &vao : vaos) {
        glDeleteVertexArrays(1, &vao);
    }
    for (unsigned int &vbo : vbos) {
        glDeleteBuffers(1, &vbo);
    }
    for (unsigned int &tex : textures) {
        glDeleteTextures(1, &tex);
    }
}

GLuint Loader::createVAO() {
    GLuint vaoId;
    glGenVertexArrays(1, &vaoId);
    this->vaos.push_back(vaoId);
    glBindVertexArray(vaoId);
    return vaoId;
}

void Loader::storeDataInAttributeList(GLuint attributeNumber, int coordinateSize, std::vector<GLfloat> positions) {
    GLuint vboId = createVBO();
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(positions[0])), &positions.front(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(attributeNumber, coordinateSize, GL_FLOAT, GL_FALSE,
                          coordinateSize * static_cast<GLsizei>(sizeof(float)), (void *) 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Loader::bindIndicesBuffer(std::vector<GLint> indices) {
    GLuint vboId;
    glGenBuffers(1, &vboId);
    vbos.push_back(vboId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(indices[0])),
                 &indices.front(), GL_STATIC_DRAW);
}

/**
 * @brief Used for loading dynamic data allocation of for fonts.
 *
 * @param vboNumber
 */
void Loader::initDynamicAttributeList(GLuint vboNumber) {
    glBindBuffer(GL_ARRAY_BUFFER, vboNumber);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * FontModel::kVertexCount * FontModel::kVertexSize, NULL,
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, FontModel::kVertexSize, GL_FLOAT, GL_FALSE,
                          FontModel::kVertexSize * static_cast<GLsizei>(sizeof(float)), (void *) 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Loader::unbindVAO() {
    glBindVertexArray(0);
}

unsigned int Loader::loadCubeMap(std::vector<std::string> faces) {

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        auto bytes = FileSystem::readAllBytes(faces[i]);
        unsigned char *data = nullptr;
        if (!bytes.empty()) {
            data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                         &width, &height, &nrChannels, 0);
        }
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        } else {
            // data is nullptr on failure — no stbi_image_free needed
            std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    textures.push_back(textureID);
    return textureID;
}

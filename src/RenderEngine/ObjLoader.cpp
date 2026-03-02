//
// Created by Joseph Alai on 7/6/21.
//

#include "ObjLoader.h"

#include "../Util/FileSystem.h"

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

using namespace std;

/**
 * @brief Loads an ASSIMP model, and returns the vertices,
 *        normals, textureCoords, and kBboxIndices in vector format. It also gets the min and max of the mesh
 *        so as to programmitcally generate a Bounding Box from it, if needed.
 * @param path
 * @return
 */
ModelData OBJLoader::loadAssImp(
        string path
) {
    std::vector<int> indices;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    BoxData box;

    Assimp::Importer importer;

    const aiScene *scene = importer.ReadFile(FileSystem::Path(path),
                                             0/*aiProcess_JoinIdenticalVertices | aiProcess_SortByPType*/);
    if (!scene) {
        fprintf(stderr, importer.GetErrorString());
        getchar();
    }
    const aiMesh *mesh = scene->mMeshes[0]; // In this simple example code we always use the 1rst mesh (in OBJ files there is often only one anyway)

    // Fill vertices positions
    vertices.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        aiVector3D pos = mesh->mVertices[i];
        vertices.emplace_back(pos.x, pos.y, pos.z);
        // Bounding Box
        // Update lower-left-front corner of BB
        box.vLowerLeftFront.x = min(box.vLowerLeftFront.x, pos.x);
        box.vLowerLeftFront.y = min(box.vLowerLeftFront.y, pos.y);
        box.vLowerLeftFront.z = max(box.vLowerLeftFront.z, pos.z);
        // Update upper-right-back corner of BB
        box.vUpperRightBack.x = max(box.vUpperRightBack.x, pos.x);
        box.vUpperRightBack.y = max(box.vUpperRightBack.y, pos.y);
        box.vUpperRightBack.z = min(box.vUpperRightBack.z, pos.z);
    }

    // Fill vertices texture coordinates
    uvs.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        aiVector3D UVW = mesh->mTextureCoords[0][i]; // Assume only 1 set of UV coords; AssImp supports 8 UV sets.
        uvs.emplace_back(UVW.x, UVW.y);
    }

    // Fill vertices normals
    normals.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        aiVector3D n = mesh->mNormals[i];
        normals.emplace_back(n.x, n.y, n.z);
    }


    // Fill face kBboxIndices
    indices.reserve(3 * mesh->mNumFaces);
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        // Assume the model has only triangles.
        indices.push_back(static_cast<int>(mesh->mFaces[i].mIndices[0]));
        indices.push_back(static_cast<int>(mesh->mFaces[i].mIndices[1]));
        indices.push_back(static_cast<int>(mesh->mFaces[i].mIndices[2]));
    }

    vector<GLfloat> outVertices, outUvs, outNormals;
    vector<GLint> outIndices(indices.begin(), indices.end());
    for (int i = 0; i < vertices.size(); i++) {
        outVertices.push_back(vertices[i].x);
        outVertices.push_back(vertices[i].y);
        outVertices.push_back(vertices[i].z);
        outUvs.push_back(uvs[i].x);
        outUvs.push_back(uvs[i].y);
        outNormals.push_back(normals[i].x);
        outNormals.push_back(normals[i].y);
        outNormals.push_back(normals[i].z);
    }
    ModelData data(outVertices, outUvs, outNormals, indices, box.vLowerLeftFront, box.vUpperRightBack);
    return data;
    // The "scene" pointer will be deleted automatically by "importer"
}


/**
 * @brief loadObjModel loads in an .obj file, and returns the vertices,
 *        normals, textureCoords, and kBboxIndices in vector format. It also gets the min and max of the mesh
 *        so as to programmitcally generate a Bounding Box from it, if needed.
 * @param filename
 * @return
 */
ModelData OBJLoader::loadObjModel(const std::string &filename) {
    std::vector<Vertex *> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> textures;
    std::vector<GLint> indices;
    glm::vec3 vMin;
    glm::vec3 vMax;

    auto copy = FileSystem::Model(filename);
    FILE *file = std::fopen(std::move(FileSystem::Model(filename)).c_str(), "r");
    if (file == nullptr) {
        printf("Impossible to open the file: %s !\n", copy.c_str());
        return ModelData{};
    }

    while (true) {
        char lineHeader[128];
        // read the first word of the line
        int res = fscanf(file, "%s", lineHeader);
        if (res == EOF)
            break; // EOF = End Of File. Quit the loop.
        if (strcmp(lineHeader, "v") == 0) {
            glm::vec3 vertex;
            fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
            auto newVertex = new Vertex(static_cast<int>(vertices.size()), vertex);
            vertices.push_back(newVertex);

            // Bounding Box
            // Update lower-left-front corner of BB
            vMin.x = min(vMin.x, vertex.x);
            vMin.y = min(vMin.y, vertex.y);
            vMin.z = max(vMin.z, vertex.z);
            // Update upper-right-back corner of BB
            vMax.x = max(vMax.x, vertex.x);
            vMax.y = max(vMax.y, vertex.y);
            vMax.z = min(vMax.z, vertex.z);

        } else if (strcmp(lineHeader, "vt") == 0) {
            glm::vec2 uv;
            fscanf(file, "%f %f\n", &uv.x, &uv.y);
            textures.push_back(uv);
        } else if (strcmp(lineHeader, "vn") == 0) {
            glm::vec3 normal;
            fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
            normals.push_back(normal);

        } else if (strcmp(lineHeader, "f") == 0) {

            unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
            int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0],
                                 &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2],
                                 &uvIndex[2], &normalIndex[2]);

            if (matches != 9) {
                printf("File can't be read by our simple parser : ( Try exporting with other options\n");
            }
            processVertex(static_cast<float>(vertexIndex[0]), static_cast<float>(uvIndex[0]),
                          static_cast<float>(normalIndex[0]), &vertices, &indices);
            processVertex(static_cast<float>(vertexIndex[1]), static_cast<float>(uvIndex[1]),
                          static_cast<float>(normalIndex[1]), &vertices, &indices);
            processVertex(static_cast<float>(vertexIndex[2]), static_cast<float>(uvIndex[2]),
                          static_cast<float>(normalIndex[2]), &vertices, &indices);
        }
    }
    std::fclose(file);

    removeUnusedVertices(&vertices);

    vector<float> verticesArray(vertices.size() * 3);
    verticesArray.reserve(vertices.size() * 3);
    vector<float> texturesArray(vertices.size() * 2);
    texturesArray.reserve(vertices.size() * 2);
    vector<float> normalsArray(vertices.size() * 3);
    normalsArray.reserve(vertices.size() * 3);

    // converts data to an array and returns the furthest point
    convertDataToArrays(vertices, textures, normals, &verticesArray, &texturesArray, &normalsArray);

    ModelData data(verticesArray, texturesArray, normalsArray, indices, vMin, vMax);
    return data;
}

void OBJLoader::processVertex(float vertex, float uv, float normal, vector<Vertex *> *vertices, vector<int> *indices) {
    int index = static_cast<int>(vertex) - 1;
    Vertex *currentVertex = (*vertices)[index];

    int textureIndex = static_cast<int>(uv) - 1;
    int normalIndex = static_cast<int>(normal) - 1;
    if (!currentVertex->isSet()) {
        currentVertex->setTextureIndex(textureIndex);
        currentVertex->setNormalIndex(normalIndex);
        indices->push_back(index);
    } else {
        dealWithAlreadyProcessedVertex(currentVertex, textureIndex, normalIndex, indices,
                                       vertices);
    }
}

int *OBJLoader::convertIndicesListToArray(vector<int> indices) {
    int *indicesArray = new int[indices.size()];
    for (int i = 0; i < indices.size(); i++) {
        indicesArray[i] = indices[i];
    }
    return indicesArray;
}

/**
 * @brief Converts the vector data of a loaded object (multiple vectors) into three single arrays to be loaded into
 *        OpenGL easily.
 *
 * @param vertices
 * @param textures
 * @param normals
 * @param verticesArray
 * @param texturesArray
 * @param normalsArray
 * @return
 */
float OBJLoader::convertDataToArrays(vector<Vertex *> vertices, vector<glm::vec2> textures,
                                     vector<glm::vec3> normals, vector<float> *verticesArray,
                                     vector<float> *texturesArray,
                                     vector<float> *normalsArray) {
    float furthestPoint = 0;
    for (int i = 0; i < vertices.size(); i++) {
        auto currentVertex = (vertices)[i];
        if (currentVertex->getLength() > furthestPoint) {
            furthestPoint = currentVertex->getLength();
        }
        glm::vec3 position = currentVertex->getPosition();
        glm::vec2 textureCoord = (textures)[currentVertex->getTextureIndex()];
        glm::vec3 normalVector = (normals)[currentVertex->getNormalIndex()];
        (*verticesArray)[i * 3] = position.x;
        (*verticesArray)[i * 3 + 1] = position.y;
        (*verticesArray)[i * 3 + 2] = position.z;
        (*texturesArray)[i * 2] = textureCoord.x;
        (*texturesArray)[i * 2 + 1] = 1 - textureCoord.y;
        (*normalsArray)[i * 3] = normalVector.x;
        (*normalsArray)[i * 3 + 1] = normalVector.y;
        (*normalsArray)[i * 3 + 2] = normalVector.z;
    }
    return furthestPoint;
}

void OBJLoader::removeUnusedVertices(vector<Vertex *> *vertices) {
    for (Vertex *vertex:*vertices) {
        if (!vertex->isSet()) {
            vertex->setTextureIndex(0);
            vertex->setNormalIndex(0);
        }
    }
}

void OBJLoader::dealWithAlreadyProcessedVertex(Vertex *previousVertex, int newTextureIndex,
                                               int newNormalIndex, vector<int> *indices, vector<Vertex *> *vertices) {
    if (previousVertex->hasSameTextureAndNormal(newTextureIndex, newNormalIndex)) {
        indices->push_back(previousVertex->getIndex());
    } else {
        Vertex *anotherVertex = previousVertex->getDuplicateVertex();
        if (anotherVertex != nullptr) {
            dealWithAlreadyProcessedVertex(anotherVertex, newTextureIndex, newNormalIndex,
                                           indices, vertices);
        } else {
            auto duplicateVertex = new Vertex(static_cast<int>(vertices->size()), previousVertex->getPosition());
            duplicateVertex->setTextureIndex(newTextureIndex);
            duplicateVertex->setNormalIndex(newNormalIndex);
            previousVertex->setDuplicateVertex(duplicateVertex);
            vertices->push_back(duplicateVertex);
            indices->push_back(duplicateVertex->getIndex());
        }

    }
}

/**
 * @brief Creates a bounding box, either a mesh, or programmatically generates a box from a mesh.
 *
 * @param filename
 * @param boxType
 * @param boundType
 * @return
 */
BoundingBoxData OBJLoader::loadBoundingBox(const string &filename, ClickBoxTypes boxType = ClickBoxTypes::BOX,
                                           BoundTypes boundType = BoundTypes::AABB) {
    auto data = loadObjModel(filename);
    AABB br(boundType);
    if (boundType == BoundTypes::AABB) {
        br.min = data.getMin();
        br.max = data.getMax();
    } else {
        OBJLoader::generateSphere(data, br);
    }

    return BoundingBoxData(br, data.getVertices(), data.getIndices());
}

/**
 * @brief Loads the bounding box based on the mesh given.
 *
 *        Have the choice of creating a rectangular bounding box programmatically, based on the max and min of the mesh,
 *        or loading a mesh as a bounding box. MESH loading BoundingBox is useful when the mesh is something like a
 *        tree, and when the box is rendered, it covers an entire box around the tree from top to bottom. It covers the
 *        volume of not only the trunk and the leaves on top, but from the leaves vertically all the way to the ground,
 *        making it so they cannot click the empty space between the leaves and the ground, because the box is taking up
 *        that space too. So creating a low-poly tree mesh (two quads, one to cover the leaves on top, and one to cover
 *        the trunk), they can easily create a bounding box that is not a huge monolith, and that lets the player click
 *        in all the space accurately around the tree.
 *
 * @param data
 * @param boxType COLORED_BOX or MESH
 * @param boundType
 * @return
 */
BoundingBoxData OBJLoader::loadBoundingBox(ModelData &data, ClickBoxTypes boxType = ClickBoxTypes::BOX,
                                           BoundTypes boundType = BoundTypes::AABB) {
    AABB br(boundType);
    std::vector<float> vertices;
    std::vector<int> indices = {
            0, 1, 2, 3, 8, // Front wall
            4, 5, 6, 7, 8, // Back wall
            4, 0, 6, 2, 8, // Left wall
            1, 5, 3, 7, 8, // Right wall
            2, 3, 6, 7, 8, // Top wall
            0, 1, 4, 5     // Bottom wall
    };

    // asks for a mesh instead of a box (clickBox)
    if (boxType == ClickBoxTypes::MESH) {
        // asks for a BoundType AABB
        vertices = data.getVertices();
        indices = data.getIndices();
    } else { // asks for a COLORED_BOX clickBoxType
        // asks for a BoundType AABB (box)
        vertices = OBJLoader::generateBox(data.getMin(), data.getMax());
    }

    if (boundType == BoundTypes::AABB) {
        // box around the mesh
        br.min = data.getMin();
        br.max = data.getMax();
    } else { // asks for BoundType Sphere
        // sphere around mesh
        OBJLoader::generateSphere(data, br);
    }

    return BoundingBoxData(br, vertices, indices);
}

void OBJLoader::generateSphere(ModelData &data, AABB &br) {
    br.center = AABB(data.getMin(), data.getMax()).calculateCenter();
    float maxRadiusSquared = 0.0f;

    for (unsigned int i = 0; i < data.getVertices().size(); i++) {
        float radiusSquared = 0.0f; // distance for this vertex
        for (int j = 0; j < 3; ++j) {
            radiusSquared += ((data.getVertices()[i]) - br.center[j]) * ((data.getVertices()[i]) - br.center[j]);
        }
        if (radiusSquared > maxRadiusSquared) {
            // found new squared radius
            // a^2 * b*2 > |a| > |b|
            maxRadiusSquared = radiusSquared;
        }
    }
    br.radius = sqrt(maxRadiusSquared);
}

/**
 * @brief Generates a bounding box programmatically, based on the min and max vertices values of x, y, and z.
 *
 * @param min
 * @param max
 * @return
 */
vector<float> OBJLoader::generateBox(glm::vec3 min, glm::vec3 max) {
    vector<float> vertices = {
            // Front wall of bounding box
            min.x, min.y, min.z,
            max.x, min.y, min.z,
            min.x, max.y, min.z,
            max.x, max.y, min.z,

            // Back wall of bounding box
            min.x, min.y, max.z,
            max.x, min.y, max.z,
            min.x, max.y, max.z,
            max.x, max.y, max.z
    };
    return vertices;
}

/**
 * @brief TODO: Fix this, it doesn't work at all
 * @param data
 * @param boxType
 * @param boundingType
 * @return
 */
BoundingBoxData OBJLoader::loadBoundingBox(AssimpMesh *data, ClickBoxTypes boxType = ClickBoxTypes::BOX,
                                           BoundTypes boundingType = BoundTypes::AABB) {
    BoxData box;
    for (auto data : data->meshes) {
        const vector<float> &vertex = data.getVertices();

        for (int i = 0; i < vertex.size(); i += 3) {
            // Bounding Box
            // Update lower-left-front corner of BB
            box.vLowerLeftFront.x = min(box.vLowerLeftFront.x, vertex[i]);
            box.vLowerLeftFront.y = min(box.vLowerLeftFront.y, vertex[i + 1]);
            box.vLowerLeftFront.z = max(box.vLowerLeftFront.z, vertex[i + 2]);
            // Update upper-right-back corner of BB
            box.vUpperRightBack.x = max(box.vUpperRightBack.x, vertex[i]);
            box.vUpperRightBack.y = max(box.vUpperRightBack.y, vertex[i + 1]);
            box.vUpperRightBack.z = min(box.vUpperRightBack.z, vertex[i + 2]);
        }
    }
    std::vector<float> vertices = {
            // Front wall of bounding box
            box.vLowerLeftFront.x, box.vLowerLeftFront.y, box.vLowerLeftFront.z,
            box.vUpperRightBack.x, box.vLowerLeftFront.y, box.vLowerLeftFront.z,
            box.vLowerLeftFront.x, box.vUpperRightBack.y, box.vLowerLeftFront.z,
            box.vUpperRightBack.x, box.vUpperRightBack.y, box.vLowerLeftFront.z,

            // Back wall of bounding box
            box.vLowerLeftFront.x, box.vLowerLeftFront.y, box.vUpperRightBack.z,
            box.vUpperRightBack.x, box.vLowerLeftFront.y, box.vUpperRightBack.z,
            box.vLowerLeftFront.x, box.vUpperRightBack.y, box.vUpperRightBack.z,
            box.vUpperRightBack.x, box.vUpperRightBack.y, box.vUpperRightBack.z
    };
    std::vector<int> indices = {
            0, 1, 2, 3, 8, // Front wall
            4, 5, 6, 7, 8, // Back wall
            4, 0, 6, 2, 8, // Left wall
            1, 5, 3, 7, 8, // Right wall
            2, 3, 6, 7, 8, // Top wall
            0, 1, 4, 5     // Bottom wall
    };

    AABB br(boundingType);
    // box around the mesh
    br.min = box.vLowerLeftFront;
    br.max = box.vUpperRightBack;

    return BoundingBoxData(br, vertices, indices);
}

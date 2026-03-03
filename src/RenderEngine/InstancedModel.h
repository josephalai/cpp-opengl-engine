// src/RenderEngine/InstancedModel.h
// Wraps a RawModel + texture with a per-instance transform VBO.

#ifndef ENGINE_INSTANCEDMODEL_H
#define ENGINE_INSTANCEDMODEL_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <vector>
#include <glm/glm.hpp>
#include "../Models/RawModel.h"

class InstancedModel {
public:
    /// vaoId / vertexCount / indexCount from the original static mesh.
    GLuint vaoID       = 0;
    int    indexCount  = 0;
    GLuint textureID   = 0;

    InstancedModel(GLuint vao, int idxCount, GLuint tex);

    /// Attach a dynamic instance VBO to the VAO (call once after construction).
    void setupInstanceVBO();

    /// Upload new instance transforms (model matrices) to the GPU.
    void setInstances(const std::vector<glm::mat4>& transforms);

    /// Add a single instance transform to the pending list.
    void addInstance(const glm::mat4& transform) { instances.push_back(transform); }

    int  getInstanceCount() const { return static_cast<int>(instances.size()); }
    void clear()                  { instances.clear(); }

    GLuint getInstanceVBO() const { return instanceVBO; }

    const std::vector<glm::mat4>& getInstances() const { return instances; }

private:
    GLuint              instanceVBO = 0;
    std::vector<glm::mat4> instances;
};

#endif // ENGINE_INSTANCEDMODEL_H

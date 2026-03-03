// src/Animation/AnimatedModel.h
// Combines skeletal mesh data, a Skeleton, and a list of AnimationClips.

#ifndef ENGINE_ANIMATEDMODEL_H
#define ENGINE_ANIMATEDMODEL_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "Skeleton.h"
#include "AnimationClip.h"

/// Vertex format for skeletal meshes — up to 4 bone influences per vertex.
struct AnimatedVertex {
    glm::vec3  position;
    glm::vec3  normal;
    glm::vec2  texCoords;
    glm::ivec4 boneIDs     = glm::ivec4(0);
    glm::vec4  boneWeights = glm::vec4(0.0f);
};

/// One sub-mesh (draw call) inside an AnimatedModel.
struct AnimatedMesh {
    std::vector<AnimatedVertex> vertices;
    std::vector<unsigned int>   indices;
    unsigned int textureID = 0;
    GLuint VAO = 0, VBO = 0, EBO = 0;
};

class AnimatedModel {
public:
    std::vector<AnimatedMesh> meshes;
    Skeleton                  skeleton;
    std::vector<AnimationClip> clips;
    std::string               directory;

    /// Applied in model space before the entity's world transform.
    /// Set automatically from Assimp metadata when the file uses a
    /// non-Y-up coordinate system (e.g. Z-up FBX from Blender).
    glm::mat4 coordinateCorrection = glm::mat4(1.0f);

    /// Returns the index of the named clip, or -1 if not found.
    int getClipIndex(const std::string& name) const;

    /// Upload all mesh data to OpenGL (VAO/VBO/EBO). Call once after loading.
    void setupMeshes();

    void cleanUp();

private:
    void setupMesh(AnimatedMesh& mesh);
};

#endif // ENGINE_ANIMATEDMODEL_H

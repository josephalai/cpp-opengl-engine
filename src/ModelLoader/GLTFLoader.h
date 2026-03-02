// src/ModelLoader/GLTFLoader.h
// Loads glTF 2.0 / GLB models via Assimp, extracting PBR materials,
// tangents, embedded/external textures, and optional skeletal animation data.

#ifndef ENGINE_GLTFLOADER_H
#define ENGINE_GLTFLOADER_H

#include <string>
#include "../Animation/AnimatedModel.h"

struct GLTFMaterial {
    glm::vec3 baseColorFactor    = glm::vec3(1.0f);
    float     metallicFactor     = 0.0f;
    float     roughnessFactor    = 0.5f;
    unsigned int baseColorTexture   = 0;
    unsigned int metallicRoughTex   = 0;
    unsigned int normalTexture       = 0;
};

/// A fully loaded glTF asset ready for rendering.
struct GLTFAsset {
    AnimatedModel*   model    = nullptr;  ///< Mesh + skeleton + clips (owned).
    GLTFMaterial     material;
    bool             hasSkeleton   = false;
    bool             hasAnimations = false;
};

class GLTFLoader {
public:
    /// Load a .gltf or .glb file.  Returns nullptr on failure.
    static GLTFAsset* load(const std::string& path);
};

#endif // ENGINE_GLTFLOADER_H

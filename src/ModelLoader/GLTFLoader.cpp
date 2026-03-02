// src/ModelLoader/GLTFLoader.cpp
// Thin wrapper around AnimationLoader that also extracts PBR material properties.

#include "GLTFLoader.h"
#include "../Animation/AnimationLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <iostream>

GLTFAsset* GLTFLoader::load(const std::string& path) {
    // Delegate mesh/skeleton/animation loading to AnimationLoader
    AnimatedModel* model = AnimationLoader::load(path);
    if (!model) return nullptr;

    auto* asset = new GLTFAsset();
    asset->model         = model;
    asset->hasSkeleton   = model->skeleton.getBoneCount() > 0;
    asset->hasAnimations = !model->clips.empty();

    // Re-open the scene just to read PBR material properties
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs);

    if (scene && scene->mNumMaterials > 0) {
        aiMaterial* mat = scene->mMaterials[0];

        // baseColorFactor — try PBR key first, fall back to classic diffuse
        aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
#if defined(AI_MATKEY_BASE_COLOR)
        if (mat->Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
#endif
            mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        asset->material.baseColorFactor = glm::vec3(color.r, color.g, color.b);

        // metallicFactor
        float metallic = 0.0f;
#if defined(AI_MATKEY_METALLIC_FACTOR)
        mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
#endif
        asset->material.metallicFactor = metallic;

        // roughnessFactor
        float roughness = 0.5f;
#if defined(AI_MATKEY_ROUGHNESS_FACTOR)
        mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
#endif
        asset->material.roughnessFactor = roughness;
    }

    return asset;
}

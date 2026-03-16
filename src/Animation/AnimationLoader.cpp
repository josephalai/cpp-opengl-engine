// src/Animation/AnimationLoader.cpp
// Uses Assimp to load mesh bone weights, bone hierarchy, and animation clips.

#include "AnimationLoader.h"
#include "../Libraries/images/stb_image.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <cmath>

// ---- helpers ----------------------------------------------------------------

static glm::mat4 toGlm(const aiMatrix4x4& m) {
    // Assimp matrices are row-major; GLM is column-major — transpose.
    return glm::transpose(glm::make_mat4(&m.a1));
}

static unsigned int loadTextureFromFile(const std::string& path,
                                        const std::string& directory,
                                        const aiScene*     scene) {
    unsigned int texID = 0;
    glGenTextures(1, &texID);
    int w, h, ch;
    unsigned char* data       = nullptr;
    bool           isManuallyAllocated = false; // true when we used new[] to allocate data

    // GLB / embedded texture: Assimp uses paths like "*0", "*1", …
    if (!path.empty() && path[0] == '*') {
        const aiTexture* embTex = scene ? scene->GetEmbeddedTexture(path.c_str()) : nullptr;
        if (embTex) {
            if (embTex->mHeight == 0) {
                // Compressed data (PNG, JPEG, …) stored as a raw byte array
                data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(embTex->pcData),
                    static_cast<int>(embTex->mWidth),
                    &w, &h, &ch, 0);
            } else {
                // Uncompressed ARGB8888 pixel data — re-order to RGBA for OpenGL
                w  = static_cast<int>(embTex->mWidth);
                h  = static_cast<int>(embTex->mHeight);
                ch = 4;
                auto* src  = reinterpret_cast<const unsigned char*>(embTex->pcData);
                auto* rgba = new unsigned char[w * h * 4];
                for (int i = 0; i < w * h; ++i) {
                    rgba[i * 4 + 0] = src[i * 4 + 2]; // R
                    rgba[i * 4 + 1] = src[i * 4 + 1]; // G
                    rgba[i * 4 + 2] = src[i * 4 + 0]; // B
                    rgba[i * 4 + 3] = src[i * 4 + 3]; // A
                }
                data         = rgba;
                isManuallyAllocated = true;
            }
        }
        if (!data) {
            std::cerr << "[AnimationLoader] Embedded texture failed: " << path << "\n";
            glDeleteTextures(1, &texID);
            return 0;
        }
    } else {
        // External texture file on disk
        std::string filename = directory + '/' + path;
        data = stbi_load(filename.c_str(), &w, &h, &ch, 0);
        if (!data) {
            std::cerr << "[AnimationLoader] Texture failed: " << filename << "\n";
            return texID;
        }
    }

    GLenum fmt = (ch == 1) ? GL_RED : (ch == 3) ? GL_RGB : GL_RGBA;
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (isManuallyAllocated) {
        delete[] data;
    } else {
        stbi_image_free(data);
    }
    return texID;
}

// ---- bone hierarchy ---------------------------------------------------------

static void buildBoneHierarchy(const aiNode* aiNode,
                                Bone* parent,
                                Skeleton& skeleton,
                                const glm::mat4& nonBoneAccum = glm::mat4(1.0f)) {
    std::string nodeName(aiNode->mName.C_Str());
    Bone* current = skeleton.getBoneByName(nodeName);

    glm::mat4 childAccum;

    if (current) {
        // Initialise to the bind-pose local transform so that in the rest state
        // globalTransform * offsetMatrix == identity (correct bind-pose rendering).
        // The animation system will overwrite localTransform each frame for bones
        // that have keyframe channels; bones without channels keep this value,
        // which is the correct bind-pose position.
        current->localTransform = toGlm(aiNode->mTransformation);

        // Attach to parent if one exists
        if (parent) {
            parent->children.push_back(current);
            // Preserve accumulated non-bone ancestor transforms between this
            // bone and its parent bone (e.g. intermediate Armature nodes in
            // Meshy GLBs).  These transforms are static and not overwritten
            // by animation keyframes, so they must be applied separately in
            // Skeleton::computeRecursive().
            current->nonBoneParentTransform = nonBoneAccum;
        } else if (!skeleton.root) {
            skeleton.root = current;
            // Store the accumulated non-bone ancestor transforms from the
            // scene root down to this bone — e.g. the Armature node that
            // Meshy exports above the Hips bone.
            skeleton.rootTransform = nonBoneAccum;
        }

        // Reset accumulation — children of a bone start fresh
        childAccum = glm::mat4(1.0f);
    } else {
        // Non-bone node: accumulate its transform for descendant bones
        childAccum = nonBoneAccum * toGlm(aiNode->mTransformation);
    }

    for (unsigned int i = 0; i < aiNode->mNumChildren; ++i) {
        buildBoneHierarchy(aiNode->mChildren[i], current ? current : parent,
                           skeleton, childAccum);
    }
}

// ---- mesh processing --------------------------------------------------------

static AnimatedMesh processMesh(aiMesh* mesh, const aiScene* scene,
                                  const std::string& directory,
                                  std::unordered_map<std::string, int>& boneIndexMap) {
    AnimatedMesh result;

    // Vertices
    result.vertices.resize(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        AnimatedVertex& v = result.vertices[i];
        v.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        if (mesh->HasNormals())
            v.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
        if (mesh->mTextureCoords[0])
            v.texCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        v.boneIDs     = glm::ivec4(0);
        v.boneWeights = glm::vec4(0.0f);
    }

    // Indices
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        for (unsigned int j = 0; j < mesh->mFaces[i].mNumIndices; ++j)
            result.indices.push_back(mesh->mFaces[i].mIndices[j]);
    }

    // Bone weights — up to 4 influences per vertex
    for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
        aiBone* aiBone = mesh->mBones[b];
        std::string boneName(aiBone->mName.C_Str());

        int boneID = -1;
        auto it = boneIndexMap.find(boneName);
        if (it != boneIndexMap.end()) {
            boneID = it->second;
        }

        for (unsigned int w = 0; w < aiBone->mNumWeights; ++w) {
            unsigned int vertIdx = aiBone->mWeights[w].mVertexId;
            float        weight  = aiBone->mWeights[w].mWeight;

            if (vertIdx >= result.vertices.size()) continue;
            AnimatedVertex& v = result.vertices[vertIdx];
            // Fill the first empty slot (weight == 0)
            for (int slot = 0; slot < 4; ++slot) {
                if (v.boneWeights[slot] == 0.0f) {
                    v.boneIDs[slot]     = boneID;
                    v.boneWeights[slot] = weight;
                    break;
                }
            }
        }
    }

    // Normalize bone weights so they sum to 1.0.  Some exporters (including
    // Meshy) can produce weights that don't sum exactly to 1.0 due to
    // floating-point precision or partial coverage.  Without normalization
    // the skinned vertex would be scaled up or down, causing visual artifacts.
    constexpr float kWeightEpsilon = 1e-5f;
    for (auto& v : result.vertices) {
        float sum = v.boneWeights.x + v.boneWeights.y
                  + v.boneWeights.z + v.boneWeights.w;
        if (sum > 0.0f && std::abs(sum - 1.0f) > kWeightEpsilon) {
            v.boneWeights /= sum;
        }
    }

    // Texture — Assimp maps glTF baseColorTexture to aiTextureType_DIFFUSE
    // in all versions, so a single check is sufficient for PBR materials.
    if (mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            result.textureID = loadTextureFromFile(texPath.C_Str(), directory, scene);
        }
    }

    return result;
}

// ---- public interface -------------------------------------------------------

AnimatedModel* AnimationLoader::load(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs     | aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "[AnimationLoader] Assimp error: " << importer.GetErrorString() << "\n";
        return nullptr;
    }

    auto* model = new AnimatedModel();
    model->directory = path.substr(0, path.find_last_of('/'));

    // 1) Build bone index map from all meshes
    std::unordered_map<std::string, int> boneIndexMap;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];
        for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
            std::string boneName(mesh->mBones[b]->mName.C_Str());
            if (boneIndexMap.find(boneName) == boneIndexMap.end()) {
                int newID = static_cast<int>(boneIndexMap.size());
                if (newID >= MAX_BONES) break;
                boneIndexMap[boneName] = newID;

                glm::mat4 offset = toGlm(mesh->mBones[b]->mOffsetMatrix);
                auto* bone = new Bone(boneName, newID, offset);
                model->skeleton.bones.push_back(bone);
                model->skeleton.bonesByName[boneName] = bone;
            }
        }
    }

    // 2) Build bone hierarchy from the scene node tree
    buildBoneHierarchy(scene->mRootNode, nullptr, model->skeleton);

    // If no root bone was identified (no named bones matched nodes), use first
    if (!model->skeleton.root && !model->skeleton.bones.empty()) {
        model->skeleton.root = model->skeleton.bones[0];
    }

    // 3) Process meshes
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        model->meshes.push_back(processMesh(scene->mMeshes[m], scene,
                                             model->directory, boneIndexMap));
    }

    // 4) Load animation clips
    for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
        aiAnimation* anim = scene->mAnimations[a];
        float tps = (anim->mTicksPerSecond > 0.0) ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;
        AnimationClip clip(anim->mName.C_Str(), static_cast<float>(anim->mDuration), tps);

        for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
            aiNodeAnim* ch = anim->mChannels[c];
            BoneAnimation ba;

            for (unsigned int k = 0; k < ch->mNumPositionKeys; ++k)
                ba.positions.push_back({ { ch->mPositionKeys[k].mValue.x,
                                           ch->mPositionKeys[k].mValue.y,
                                           ch->mPositionKeys[k].mValue.z },
                                         static_cast<float>(ch->mPositionKeys[k].mTime) });

            for (unsigned int k = 0; k < ch->mNumRotationKeys; ++k) {
                const auto& q = ch->mRotationKeys[k].mValue;
                ba.rotations.push_back({
                    glm::quat(q.w, q.x, q.y, q.z),
                    static_cast<float>(ch->mRotationKeys[k].mTime)
                });
            }

            for (unsigned int k = 0; k < ch->mNumScalingKeys; ++k)
                ba.scales.push_back({ { ch->mScalingKeys[k].mValue.x,
                                        ch->mScalingKeys[k].mValue.y,
                                        ch->mScalingKeys[k].mValue.z },
                                      static_cast<float>(ch->mScalingKeys[k].mTime) });

            clip.channels[ch->mNodeName.C_Str()] = std::move(ba);
        }

        model->clips.push_back(std::move(clip));
    }

    // 5) Detect coordinate system and set corrective rotation if needed.
    // Assimp FBX metadata reports UpAxis: 0=X, 1=Y, 2=Z.
    // glTF/GLB files are always Y-up (UpAxis=1) so no correction is needed.
    // For Z-up models (e.g. FBX exported from Blender without axis conversion)
    // we rotate -90° around X so the mesh's +Z maps to OpenGL's +Y.
    {
        int32_t upAxis     = 1; // default: Y-up
        int32_t upAxisSign = 1;
        if (scene->mMetaData) {
            scene->mMetaData->Get("UpAxis",     upAxis);
            scene->mMetaData->Get("UpAxisSign", upAxisSign);
        }
        if (upAxis == 2) {
            // Z-up → rotate by sign * (-90°) around X
            float angle = static_cast<float>(upAxisSign) * glm::radians(-90.0f);
            model->coordinateCorrection =
                glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 0.0f, 0.0f));
            std::cout << "[AnimationLoader] Z-up model detected — applying coordinate correction.\n";
        }
    }

    // 6) Upload to GPU
    model->setupMeshes();

    return model;
}

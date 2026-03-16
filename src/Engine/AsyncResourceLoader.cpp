// src/Engine/AsyncResourceLoader.cpp
// Two-phase animated model loading:
//   Phase 1 (worker thread): Assimp parse + stbi texture decode → CPU buffers.
//   Phase 2 (GL thread via GLUploadQueue): GL object creation + callback.

#include "AsyncResourceLoader.h"
#include "GLUploadQueue.h"
#include "../Animation/AnimatedModel.h"
#include "../Animation/AnimationClip.h"
#include "../Animation/Skeleton.h"
#include "../Animation/Bone.h"
#include "../Libraries/images/stb_image.h"
#include "../Util/FileSystem.h"

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal CPU-only representation (no GL objects)
// ---------------------------------------------------------------------------

struct RawTextureData {
    std::vector<unsigned char> pixels;
    int width    = 0;
    int height   = 0;
    int channels = 0;
    bool valid   = false;
};

struct RawMeshData {
    std::vector<AnimatedVertex> vertices;
    std::vector<unsigned int>   indices;
    RawTextureData              texture;
};

struct RawAnimatedModelData {
    std::vector<RawMeshData>  meshes;
    Skeleton                  skeleton;
    std::vector<AnimationClip> clips;
    glm::mat4                 coordinateCorrection{1.0f};
    bool                      valid = false;
};

// ---------------------------------------------------------------------------
// Helpers (CPU only — no GL calls)
// ---------------------------------------------------------------------------

static glm::mat4 toGlmAsync(const aiMatrix4x4& m) {
    return glm::transpose(glm::make_mat4(&m.a1));
}

static void buildBoneHierarchyAsync(const aiNode* node, Bone* parent, Skeleton& skeleton,
                                     const glm::mat4& nonBoneAccum = glm::mat4(1.0f)) {
    std::string name(node->mName.C_Str());
    Bone* current = skeleton.getBoneByName(name);

    glm::mat4 childAccum;

    if (current) {
        current->localTransform = toGlmAsync(node->mTransformation);
        if (parent) {
            parent->children.push_back(current);
            current->nonBoneParentTransform = nonBoneAccum;
        } else if (!skeleton.root) {
            skeleton.root = current;
            skeleton.rootTransform = nonBoneAccum;
        }
        childAccum = glm::mat4(1.0f);
    } else {
        childAccum = nonBoneAccum * toGlmAsync(node->mTransformation);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        buildBoneHierarchyAsync(node->mChildren[i], current ? current : parent,
                                skeleton, childAccum);
    }
}

static RawTextureData loadTextureCPU(const std::string& texPath,
                                     const std::string& directory,
                                     const aiScene*     scene) {
    RawTextureData result;
    int w = 0, h = 0, ch = 0;
    unsigned char* data = nullptr;

    if (!texPath.empty() && texPath[0] == '*') {
        const aiTexture* emb = scene ? scene->GetEmbeddedTexture(texPath.c_str()) : nullptr;
        if (emb) {
            if (emb->mHeight == 0) {
                data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(emb->pcData),
                    static_cast<int>(emb->mWidth), &w, &h, &ch, 0);
            } else {
                w  = static_cast<int>(emb->mWidth);
                h  = static_cast<int>(emb->mHeight);
                ch = 4;
                const unsigned char* src = reinterpret_cast<const unsigned char*>(emb->pcData);
                data = new unsigned char[w * h * 4];
                for (int i = 0; i < w * h; ++i) {
                    data[i * 4 + 0] = src[i * 4 + 2];
                    data[i * 4 + 1] = src[i * 4 + 1];
                    data[i * 4 + 2] = src[i * 4 + 0];
                    data[i * 4 + 3] = src[i * 4 + 3];
                }
            }
        }
    } else {
        std::string filename = directory + '/' + texPath;
        auto fileBytes = FileSystem::readAllBytes(filename);
        if (!fileBytes.empty()) {
            data = stbi_load_from_memory(fileBytes.data(),
                                         static_cast<int>(fileBytes.size()),
                                         &w, &h, &ch, 0);
        }
    }

    if (data && w > 0 && h > 0) {
        result.width    = w;
        result.height   = h;
        result.channels = ch;
        result.pixels.assign(data, data + w * h * ch);
        result.valid    = true;
    }
    if (data) {
        if (!texPath.empty() && texPath[0] == '*') {
            // May have been allocated with new[]
            const aiTexture* emb = scene ? scene->GetEmbeddedTexture(texPath.c_str()) : nullptr;
            if (emb && emb->mHeight != 0) {
                delete[] data;
            } else {
                stbi_image_free(data);
            }
        } else {
            stbi_image_free(data);
        }
    }
    return result;
}

static RawMeshData processMeshCPU(aiMesh* mesh, const aiScene* scene,
                                   const std::string& directory,
                                   std::unordered_map<std::string, int>& boneMap) {
    RawMeshData result;
    result.vertices.resize(mesh->mNumVertices);

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        AnimatedVertex& v = result.vertices[i];
        v.position  = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        if (mesh->HasNormals())
            v.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
        if (mesh->mTextureCoords[0])
            v.texCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        v.boneIDs     = glm::ivec4(0);
        v.boneWeights = glm::vec4(0.0f);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        for (unsigned int j = 0; j < mesh->mFaces[i].mNumIndices; ++j)
            result.indices.push_back(mesh->mFaces[i].mIndices[j]);

    for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
        aiBone* ab = mesh->mBones[b];
        std::string name(ab->mName.C_Str());
        int id = -1;
        auto it = boneMap.find(name);
        if (it != boneMap.end()) id = it->second;
        for (unsigned int w = 0; w < ab->mNumWeights; ++w) {
            unsigned int vi = ab->mWeights[w].mVertexId;
            float wt        = ab->mWeights[w].mWeight;
            if (vi >= result.vertices.size()) continue;
            AnimatedVertex& v = result.vertices[vi];
            for (int slot = 0; slot < 4; ++slot) {
                if (v.boneWeights[slot] == 0.0f) {
                    v.boneIDs[slot]     = id;
                    v.boneWeights[slot] = wt;
                    break;
                }
            }
        }
    }

    // Normalize bone weights
    for (auto& v : result.vertices) {
        float sum = v.boneWeights.x + v.boneWeights.y
                  + v.boneWeights.z + v.boneWeights.w;
        if (sum > 0.0f && std::abs(sum - 1.0f) > 1e-5f) {
            v.boneWeights /= sum;
        }
    }

    if (mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        aiString texPath;
        bool found = false;
        if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            found = true;
        }
#ifdef aiTextureType_BASE_COLOR
        if (!found && mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
            mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath);
            found = true;
        }
#endif
        if (found) {
            result.texture = loadTextureCPU(texPath.C_Str(), directory, scene);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// CPU parse (runs on worker thread)
// ---------------------------------------------------------------------------

static RawAnimatedModelData parseCPU(const std::string& path) {
    RawAnimatedModelData out;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs     | aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "[AsyncResourceLoader] Assimp error: " << importer.GetErrorString() << "\n";
        return out;
    }

    std::string directory = path.substr(0, path.find_last_of('/'));

    // Build bone index map
    std::unordered_map<std::string, int> boneMap;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];
        for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
            std::string name(mesh->mBones[b]->mName.C_Str());
            if (boneMap.find(name) == boneMap.end()) {
                int id = static_cast<int>(boneMap.size());
                if (id >= MAX_BONES) break;
                boneMap[name] = id;
                glm::mat4 offset = toGlmAsync(mesh->mBones[b]->mOffsetMatrix);
                auto* bone = new Bone(name, id, offset);
                out.skeleton.bones.push_back(bone);
                out.skeleton.bonesByName[name] = bone;
            }
        }
    }

    buildBoneHierarchyAsync(scene->mRootNode, nullptr, out.skeleton);
    if (!out.skeleton.root && !out.skeleton.bones.empty())
        out.skeleton.root = out.skeleton.bones[0];

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
        out.meshes.push_back(processMeshCPU(scene->mMeshes[m], scene, directory, boneMap));

    // Animation clips
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
                ba.rotations.push_back({ glm::quat(q.w, q.x, q.y, q.z),
                                         static_cast<float>(ch->mRotationKeys[k].mTime) });
            }
            for (unsigned int k = 0; k < ch->mNumScalingKeys; ++k)
                ba.scales.push_back({ { ch->mScalingKeys[k].mValue.x,
                                        ch->mScalingKeys[k].mValue.y,
                                        ch->mScalingKeys[k].mValue.z },
                                      static_cast<float>(ch->mScalingKeys[k].mTime) });
            clip.channels[ch->mNodeName.C_Str()] = std::move(ba);
        }
        out.clips.push_back(std::move(clip));
    }

    // Coordinate correction
    int32_t upAxis = 1, upAxisSign = 1;
    if (scene->mMetaData) {
        scene->mMetaData->Get("UpAxis",     upAxis);
        scene->mMetaData->Get("UpAxisSign", upAxisSign);
    }
    if (upAxis == 2) {
        float angle = static_cast<float>(upAxisSign) * glm::radians(-90.0f);
        out.coordinateCorrection =
            glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    out.valid = true;
    return out;
}

// ---------------------------------------------------------------------------
// GL upload (runs on main thread via GLUploadQueue)
// ---------------------------------------------------------------------------

static AnimatedModel* uploadToGPU(RawAnimatedModelData raw) {
    if (!raw.valid) return nullptr;

    auto* model = new AnimatedModel();
    model->skeleton              = std::move(raw.skeleton);
    model->clips                 = std::move(raw.clips);
    model->coordinateCorrection  = raw.coordinateCorrection;

    for (auto& rm : raw.meshes) {
        AnimatedMesh mesh;
        mesh.vertices = std::move(rm.vertices);
        mesh.indices  = std::move(rm.indices);

        // Create GL texture if we have pixel data
        if (rm.texture.valid) {
            glGenTextures(1, &mesh.textureID);
            GLenum fmt = (rm.texture.channels == 1) ? GL_RED
                       : (rm.texture.channels == 3) ? GL_RGB : GL_RGBA;
            glBindTexture(GL_TEXTURE_2D, mesh.textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt),
                         rm.texture.width, rm.texture.height, 0,
                         fmt, GL_UNSIGNED_BYTE, rm.texture.pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        model->meshes.push_back(std::move(mesh));
    }

    model->setupMeshes();
    return model;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

AsyncResourceLoader& AsyncResourceLoader::instance() {
    static AsyncResourceLoader inst;
    return inst;
}

void AsyncResourceLoader::loadAnimatedModelAsync(const std::string& path,
                                                  std::function<void(AnimatedModel*)> callback) {
    // Launch parsing on a detached worker thread.
    // The worker does CPU-only work (Assimp + stbi), then enqueues the GL
    // upload to GLUploadQueue so it runs on the main thread.
    std::thread([path, callback]() {
        RawAnimatedModelData raw = parseCPU(path);
        GLUploadQueue::instance().enqueue([raw = std::move(raw), callback]() mutable {
            AnimatedModel* model = uploadToGPU(std::move(raw));
            callback(model);
        });
    }).detach();
}

void AsyncResourceLoader::loadTextureAsync(const std::string& path,
                                            std::function<void(unsigned int)> callback) {
    std::thread([path, callback]() {
        auto fileBytes = FileSystem::readAllBytes(path);
        int w = 0, h = 0, ch = 0;
        unsigned char* data = nullptr;
        if (!fileBytes.empty()) {
            data = stbi_load_from_memory(fileBytes.data(),
                                         static_cast<int>(fileBytes.size()),
                                         &w, &h, &ch, 0);
        }
        if (!data) {
            std::cerr << "[AsyncResourceLoader] Texture failed: " << path << "\n";
            GLUploadQueue::instance().enqueue([callback]() { callback(0); });
            return;
        }
        std::vector<unsigned char> pixels(data, data + w * h * ch);
        stbi_image_free(data);

        GLUploadQueue::instance().enqueue([pixels = std::move(pixels), w, h, ch, callback]() {
            unsigned int texID = 0;
            glGenTextures(1, &texID);
            GLenum fmt = (ch == 1) ? GL_RED : (ch == 3) ? GL_RGB : GL_RGBA;
            glBindTexture(GL_TEXTURE_2D, texID);
            glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt),
                         w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            callback(texID);
        });
    }).detach();
}

void AsyncResourceLoader::shutdown() {
    // Drain any GL tasks that background threads have already enqueued.
    // Detached threads that haven't finished yet will enqueue after shutdown;
    // their tasks will be processed at program startup if the engine restarts,
    // or be silently discarded when the GL context is destroyed.
    GLUploadQueue::instance().processAll();
}

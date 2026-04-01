// src/Animation/AnimationLoader.cpp
// Uses Assimp to load mesh bone weights, bone hierarchy, and animation clips.
// Also provides modular loaders (loadSkin / loadExternalAnimation) that support
// the split-file MMO pipeline produced by AssetForge.py.

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
                                Skeleton& skeleton) {
    std::string nodeName(aiNode->mName.C_Str());
    Bone* current = skeleton.getBoneByName(nodeName);

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
        } else if (!skeleton.root) {
            skeleton.root = current;
        }
    }

    for (unsigned int i = 0; i < aiNode->mNumChildren; ++i) {
        buildBoneHierarchy(aiNode->mChildren[i], current ? current : parent, skeleton);
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

    // Texture
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
        std::cerr << "[AnimationLoader::load] Assimp error for '" << path
                  << "': " << importer.GetErrorString() << "\n";
        return nullptr;
    }

    std::cout << "[AnimationLoader::load] Opening '" << path
              << "': " << scene->mNumMeshes << " mesh(es), "
              << scene->mNumAnimations << " animation(s), "
              << scene->mNumMaterials << " material(s).\n";

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
    std::cout << "[AnimationLoader::load]   Bone index map: "
              << boneIndexMap.size() << " bone(s).\n";

    // 2) Build bone hierarchy from the scene node tree
    buildBoneHierarchy(scene->mRootNode, nullptr, model->skeleton);

    // If no root bone was identified (no named bones matched nodes), use first
    if (!model->skeleton.root && !model->skeleton.bones.empty()) {
        model->skeleton.root = model->skeleton.bones[0];
        std::cout << "[AnimationLoader::load]   No root bone identified; "
                  << "defaulting to first bone: '"
                  << model->skeleton.root->name << "'.\n";
    } else if (model->skeleton.root) {
        std::cout << "[AnimationLoader::load]   Skeleton root: '"
                  << model->skeleton.root->name << "'.\n";
    } else {
        std::cerr << "[AnimationLoader::load]   WARNING: skeleton has no bones.\n";
    }

    // 3) Process meshes
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* aiM = scene->mMeshes[m];
        std::cout << "[AnimationLoader::load]   Mesh " << m << " '"
                  << aiM->mName.C_Str() << "': "
                  << aiM->mNumVertices << " verts, "
                  << aiM->mNumFaces << " faces, "
                  << aiM->mNumBones << " bone(s).\n";
        model->meshes.push_back(processMesh(aiM, scene,
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

        std::cout << "[AnimationLoader::load]   Clip " << a << " '"
                  << anim->mName.C_Str() << "': "
                  << anim->mNumChannels << " channel(s), dur="
                  << anim->mDuration << ", tps=" << tps << ".\n";
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

    std::cout << "[AnimationLoader::load] Loaded '" << path
              << "': " << model->meshes.size() << " mesh(es), "
              << model->skeleton.getBoneCount() << " bone(s), "
              << model->clips.size() << " clip(s).\n";
    return model;
}

// ---- modular loaders (MMO pipeline) -----------------------------------------

AnimatedModel* AnimationLoader::loadSkin(const std::string& skinPath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(skinPath,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs     | aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "[AnimationLoader::loadSkin] Assimp error for '"
                  << skinPath << "': " << importer.GetErrorString() << "\n";
        return nullptr;
    }

    std::cout << "[AnimationLoader::loadSkin] Opening '" << skinPath
              << "': " << scene->mNumMeshes << " mesh(es), "
              << scene->mNumMaterials << " material(s).\n";

    auto* model = new AnimatedModel();
    model->directory = skinPath.substr(0, skinPath.find_last_of('/'));

    // 1) Build bone index map from mesh bones
    std::unordered_map<std::string, int> boneIndexMap;
    bool boneLimitReached = false;
    for (unsigned int m = 0; m < scene->mNumMeshes && !boneLimitReached; ++m) {
        aiMesh* mesh = scene->mMeshes[m];
        for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
            std::string boneName(mesh->mBones[b]->mName.C_Str());
            if (boneIndexMap.find(boneName) == boneIndexMap.end()) {
                int newID = static_cast<int>(boneIndexMap.size());
                if (newID >= MAX_BONES) {
                    std::cerr << "[AnimationLoader::loadSkin] WARNING: bone limit ("
                              << MAX_BONES << ") exceeded in '" << skinPath
                              << "'. Excess bones will be ignored — some skinning may be incorrect.\n";
                    boneLimitReached = true;
                    break;  // break inner; outer loop exits via !boneLimitReached guard
                }
                boneIndexMap[boneName] = newID;

                glm::mat4 offset = toGlm(mesh->mBones[b]->mOffsetMatrix);
                auto* bone = new Bone(boneName, newID, offset);
                model->skeleton.bones.push_back(bone);
                model->skeleton.bonesByName[boneName] = bone;
            }
        }
    }

    // 2) Build bone hierarchy
    buildBoneHierarchy(scene->mRootNode, nullptr, model->skeleton);
    if (!model->skeleton.root && !model->skeleton.bones.empty())
        model->skeleton.root = model->skeleton.bones[0];

    // 3) Process meshes
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* aiM = scene->mMeshes[m];
        std::cout << "[AnimationLoader::loadSkin]   Mesh " << m << " '"
                  << aiM->mName.C_Str() << "': "
                  << aiM->mNumVertices << " verts, "
                  << aiM->mNumFaces << " faces, "
                  << aiM->mNumBones << " bone(s).\n";
        model->meshes.push_back(processMesh(aiM, scene,
                                             model->directory, boneIndexMap));
    }

    // NOTE: step 4 (animation clips) is intentionally skipped — this is a skin-only load.

    // 5) Coordinate system correction
    {
        int32_t upAxis = 1, upAxisSign = 1;
        if (scene->mMetaData) {
            scene->mMetaData->Get("UpAxis",     upAxis);
            scene->mMetaData->Get("UpAxisSign", upAxisSign);
        }
        if (upAxis == 2) {
            float angle = static_cast<float>(upAxisSign) * glm::radians(-90.0f);
            model->coordinateCorrection =
                glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 0.0f, 0.0f));
            std::cout << "[AnimationLoader::loadSkin] Z-up model detected — applying coordinate correction.\n";
        }
    }

    // 6) Upload to GPU
    model->setupMeshes();

    std::cout << "[AnimationLoader::loadSkin] Loaded '" << skinPath
              << "': " << model->meshes.size() << " mesh(es), "
              << model->skeleton.getBoneCount() << " bone(s).\n";
    return model;
}

std::shared_ptr<AnimationClip> AnimationLoader::loadExternalAnimation(
    const std::string& animPath,
    Skeleton* targetSkeleton,
    const std::string& expectedName)
{
    if (!targetSkeleton) {
        std::cerr << "[AnimationLoader::loadExternalAnimation] targetSkeleton is null.\n";
        return nullptr;
    }

    Assimp::Importer importer;
    // No mesh post-processing needed — we only care about mAnimations.
    const aiScene* scene = importer.ReadFile(animPath, aiProcess_Triangulate);

    if (!scene || !scene->mRootNode) {
        std::cerr << "[AnimationLoader::loadExternalAnimation] Assimp error: "
                  << importer.GetErrorString() << "\n";
        return nullptr;
    }

if (scene->mNumAnimations == 0) {
        std::cerr << "[AnimationLoader::loadExternalAnimation] No animations found in '"
                  << animPath << "'\n";
        return nullptr;
    }

    // --- NEW SMART CLIP SELECTION LOGIC ---
    aiAnimation* anim = nullptr;

    // If the file has multiple animations, search for the one that matches our target state name.
    if (!expectedName.empty() && scene->mNumAnimations > 1) {
        std::string targetLower = expectedName;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);

        for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
            std::string trackName = scene->mAnimations[i]->mName.C_Str();
            std::string trackLower = trackName;
            std::transform(trackLower.begin(), trackLower.end(), trackLower.begin(), ::tolower);

            // If the track name contains our target (e.g. "walk" matches "Armature|Walk")
            if (trackLower.find(targetLower) != std::string::npos) {
                anim = scene->mAnimations[i];
                break;
            }
        }
    }

    // Fallback: If no match was found, or no name was provided, grab the first one.
    if (!anim) {
        anim = scene->mAnimations[0];
        if (scene->mNumAnimations > 1 && !expectedName.empty()) {
            std::cerr << "[AnimationLoader] WARNING: '" << animPath 
                      << "' has multiple animations but none matched '" << expectedName 
                      << "'. Defaulting to track 0: " << anim->mName.C_Str() << "\n";
        }
    }
    // ---------------------------------------

    float tps = (anim->mTicksPerSecond > 0.0)
                    ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;

    auto clip = std::make_shared<AnimationClip>(
        anim->mName.C_Str(),
        static_cast<float>(anim->mDuration),
        tps);

    int matchedChannels = 0;
    for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
        aiNodeAnim* ch = anim->mChannels[c];
        std::string channelName(ch->mNodeName.C_Str());

        // Only import channels whose names map to a bone in the target skeleton.
        // This allows channels like "mixamorig:RightArm" to be matched when the
        // skeleton stores bones by that exact name, and mismatched channels
        // (e.g. root motion nodes or non-bone nodes) are cleanly ignored.
        if (!targetSkeleton->getBoneByName(channelName)) continue;

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

        clip->channels[channelName] = std::move(ba);
        ++matchedChannels;
    }

    if (matchedChannels == 0) {
        std::cerr << "[AnimationLoader::loadExternalAnimation] WARNING: no channels in '"
                  << animPath << "' matched any bone in the target skeleton ("
                  << targetSkeleton->getBoneCount() << " bones). "
                  << "Check that bone names match between the skin and animation files.\n";
    } else {
        std::cout << "[AnimationLoader::loadExternalAnimation] Loaded '"
                  << animPath << "' (" << matchedChannels << "/"
                  << anim->mNumChannels << " channels matched).\n";
    }

    return clip;
}

// ---------------------------------------------------------------------------
// loadModularPart — load an equipment/body-part GLB and remap bone indices
//                   to a master skeleton (by bone-name matching).
// ---------------------------------------------------------------------------
std::vector<AnimatedMesh> AnimationLoader::loadModularPart(
    const std::string& path,
    const Skeleton& masterSkeleton)
{
    std::vector<AnimatedMesh> result;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs     | aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "[AnimationLoader::loadModularPart] Assimp error for '"
                  << path << "': " << importer.GetErrorString() << "\n";
        return result;
    }

    std::cout << "[AnimationLoader::loadModularPart] Opening '" << path
              << "': " << scene->mNumMeshes << " mesh(es).\n";

    const std::string directory = path.substr(0, path.find_last_of('/'));

    // For each Assimp mesh, build a local→master bone remap and process vertices.
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* aiM = scene->mMeshes[m];
        std::cout << "[AnimationLoader::loadModularPart]   Mesh " << m << " '"
                  << aiM->mName.C_Str() << "': "
                  << aiM->mNumVertices << " verts, "
                  << aiM->mNumFaces << " faces, "
                  << aiM->mNumBones << " bone(s).\n";

        // 1) Build local bone-name → local-index map AND remap table.
        //    localIndex → masterIndex.  -1 means "bone not found in master".
        std::unordered_map<std::string, int> localBoneMap;
        std::unordered_map<int, int> remapTable;   // localID → masterID

        for (unsigned int b = 0; b < aiM->mNumBones; ++b) {
            std::string boneName(aiM->mBones[b]->mName.C_Str());
            int localID = static_cast<int>(b);
            localBoneMap[boneName] = localID;

            const Bone* masterBone = masterSkeleton.getBoneByName(boneName);
            if (masterBone) {
                remapTable[localID] = masterBone->id;
            } else {
                remapTable[localID] = 0;
                std::cerr << "[AnimationLoader::loadModularPart] WARNING: bone '"
                          << boneName << "' in '" << path
                          << "' has no match in master skeleton — "
                          << "vertices weighted to this bone will follow bone 0 (root). "
                          << "Ensure bone names match between the part and master skeleton.\n";
            }
        }

        // 2) Process vertices
        AnimatedMesh mesh;
        mesh.vertices.resize(aiM->mNumVertices);
        for (unsigned int i = 0; i < aiM->mNumVertices; ++i) {
            AnimatedVertex& v = mesh.vertices[i];
            v.position = { aiM->mVertices[i].x, aiM->mVertices[i].y, aiM->mVertices[i].z };
            if (aiM->HasNormals())
                v.normal = { aiM->mNormals[i].x, aiM->mNormals[i].y, aiM->mNormals[i].z };
            if (aiM->mTextureCoords[0])
                v.texCoords = { aiM->mTextureCoords[0][i].x, aiM->mTextureCoords[0][i].y };
            v.boneIDs     = glm::ivec4(0);
            v.boneWeights = glm::vec4(0.0f);
        }

        // 3) Indices
        for (unsigned int i = 0; i < aiM->mNumFaces; ++i)
            for (unsigned int j = 0; j < aiM->mFaces[i].mNumIndices; ++j)
                mesh.indices.push_back(aiM->mFaces[i].mIndices[j]);

        // 4) Bone weights — remapped to master skeleton indices
        for (unsigned int b = 0; b < aiM->mNumBones; ++b) {
            aiBone* bone = aiM->mBones[b];
            int localID = static_cast<int>(b);
            int masterID = remapTable[localID];  // always populated in loop above

            for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                unsigned int vertIdx = bone->mWeights[w].mVertexId;
                float        weight  = bone->mWeights[w].mWeight;
                if (vertIdx >= mesh.vertices.size()) continue;

                AnimatedVertex& v = mesh.vertices[vertIdx];
                for (int slot = 0; slot < 4; ++slot) {
                    if (v.boneWeights[slot] == 0.0f) {
                        v.boneIDs[slot]     = masterID;
                        v.boneWeights[slot] = weight;
                        break;
                    }
                }
            }
        }

        // 5) Texture
        if (aiM->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* mat = scene->mMaterials[aiM->mMaterialIndex];
            if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texPath;
                mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
                mesh.textureID = loadTextureFromFile(texPath.C_Str(), directory, scene);
            }
        }

        // 6) Upload to GPU
        AnimatedModel::setupMesh(mesh);

        result.push_back(std::move(mesh));
    }

    std::cout << "[AnimationLoader::loadModularPart] Loaded '" << path
              << "': " << result.size() << " sub-mesh(es), remapped to master skeleton.\n";
    return result;
}

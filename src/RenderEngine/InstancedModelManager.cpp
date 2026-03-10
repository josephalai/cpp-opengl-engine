// src/RenderEngine/InstancedModelManager.cpp

#include "InstancedModelManager.h"
#include "MasterRenderer.h"
#include "../RenderEngine/Loader.h"
#include "../RenderEngine/ObjLoader.h"
#include "../Config/PrefabManager.h"
#include "../Toolbox/Maths.h"
#include "../Util/FileSystem.h"

#include <iostream>
#include <unordered_set>

// ---------------------------------------------------------------------------
// init — scan PrefabManager for instanced prefabs and load their models
// ---------------------------------------------------------------------------

void InstancedModelManager::init(Loader* loader) {
    if (initialized_) {
        std::cerr << "[InstancedModelManager] WARNING: init() called twice — ignoring.\n";
        return;
    }
    initialized_ = true;

    const auto& pm = PrefabManager::get();

    for (const auto& id : pm.allIds()) {
        const auto& prefab = pm.getPrefab(id);
        if (prefab.is_null()) continue;

        std::string renderMode = prefab.value("render_mode", "");
        if (renderMode != "instanced") continue;

        // Read model OBJ and texture from the prefab's "model" block.
        if (!prefab.contains("model")) {
            std::cerr << "[InstancedModelManager] Prefab '" << id
                      << "' has render_mode=instanced but no \"model\" block.\n";
            continue;
        }

        const auto& modelBlock = prefab["model"];
        std::string objFile     = modelBlock.value("obj",     "");
        std::string textureFile = modelBlock.value("texture", "");

        if (objFile.empty()) {
            std::cerr << "[InstancedModelManager] Prefab '" << id
                      << "' has no OBJ file in its model block.\n";
            continue;
        }

        // Log the full resolved path so it's easy to verify the file exists.
        std::string fullObjPath = FileSystem::Model(objFile);
        std::cout << "[InstancedModelManager] Loading OBJ: " << fullObjPath << "\n";

        // Load the OBJ mesh data.
        ModelData meshData = OBJLoader::loadObjModel(objFile);
        if (meshData.getIndices().empty()) {
            std::cerr << "[InstancedModelManager] Failed to load OBJ '"
                      << fullObjPath << "' for prefab '" << id << "'.\n";
            continue;
        }

        // Upload to GPU.
        RawModel* rawModel = loader->loadToVAO(meshData);
        GLuint texId = 0;
        if (!textureFile.empty()) {
            std::string fullTexPath = FileSystem::Texture(textureFile);
            std::cout << "[InstancedModelManager] Loading texture: " << fullTexPath << "\n";
            auto* tex = loader->loadTexture(textureFile);
            if (tex) {
                texId = tex->getId();
                if (texId == 0) {
                    std::cerr << "[InstancedModelManager] WARNING: Texture '"
                              << fullTexPath << "' loaded with ID=0 (file missing?).\n";
                }
            }
        }

        // Create the InstancedModel and set up its instance VBO.
        auto* im = new InstancedModel(rawModel->getVaoId(),
                                       rawModel->getVertexCount(),
                                       texId);
        im->setupInstanceVBO();

        // Use the prefab alias as the bucket key.
        std::string alias = prefab.value("alias", id);
        buckets_[alias].model = im;

        std::cout << "[InstancedModelManager] Registered instanced prefab '"
                  << alias << "' (obj=" << objFile << ", texId=" << texId
                  << ", vao=" << rawModel->getVaoId()
                  << ", indices=" << rawModel->getVertexCount() << ")\n";
    }

    std::cout << "[InstancedModelManager] Initialized with "
              << buckets_.size() << " instanced prefab(s).\n";
}

// ---------------------------------------------------------------------------
// addInstance
// ---------------------------------------------------------------------------

void InstancedModelManager::addInstance(const std::string& alias,
                                         int64_t chunkKey,
                                         const glm::mat4& transform) {
    auto it = buckets_.find(alias);
    if (it == buckets_.end()) {
        static std::unordered_set<std::string> warnedAliases;
        if (!warnedAliases.count(alias)) {
            warnedAliases.insert(alias);
            std::cerr << "[InstancedModelManager] addInstance: unknown alias '"
                      << alias << "' — no bucket registered.\n";
        }
        return;
    }

    bool wasEmpty = it->second.chunkInstances.empty();
    it->second.chunkInstances[chunkKey].push_back(transform);

    // Log the very first instance added for each alias to confirm pipeline is working.
    if (wasEmpty) {
        int chunkX = static_cast<int>(chunkKey >> 32);
        int chunkZ = static_cast<int>(static_cast<uint32_t>(chunkKey));
        std::cout << "[InstancedModelManager] First instance of '" << alias
                  << "' added (chunk [" << chunkX << "," << chunkZ << "]).\n";
    }
}

// ---------------------------------------------------------------------------
// removeChunk
// ---------------------------------------------------------------------------

void InstancedModelManager::removeChunk(int64_t chunkKey) {
    for (auto& [alias, bucket] : buckets_) {
        bucket.chunkInstances.erase(chunkKey);
    }
}

// ---------------------------------------------------------------------------
// hasAlias
// ---------------------------------------------------------------------------

bool InstancedModelManager::hasAlias(const std::string& alias) const {
    return buckets_.count(alias) > 0;
}

// ---------------------------------------------------------------------------
// submitToRenderer — aggregate all chunk instances and submit per model
// ---------------------------------------------------------------------------

void InstancedModelManager::submitToRenderer(MasterRenderer* renderer) {
    for (auto& [alias, bucket] : buckets_) {
        if (!bucket.model) continue;

        // Aggregate transforms across all loaded chunks.
        std::vector<glm::mat4> all;
        for (const auto& [ck, matrices] : bucket.chunkInstances) {
            all.insert(all.end(), matrices.begin(), matrices.end());
        }

        // Log when instance count changes (first submission or after chunk load/unload).
        if (all.size() != bucket.lastLoggedCount) {
            bucket.lastLoggedCount = all.size();
            std::cout << "[InstancedModelManager] submitToRenderer: '"
                      << alias << "' has " << all.size() << " instance(s).\n";
        }

        if (!all.empty()) {
            renderer->processInstancedEntity(bucket.model, all);
        }
    }
}

// ---------------------------------------------------------------------------
// cleanup
// ---------------------------------------------------------------------------

void InstancedModelManager::cleanup() {
    for (auto& [alias, bucket] : buckets_) {
        delete bucket.model;
        bucket.model = nullptr;
    }
    buckets_.clear();
    initialized_ = false;
}

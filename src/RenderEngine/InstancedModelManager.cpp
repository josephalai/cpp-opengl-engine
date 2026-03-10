// src/RenderEngine/InstancedModelManager.cpp

#include "InstancedModelManager.h"
#include "MasterRenderer.h"
#include "../RenderEngine/Loader.h"
#include "../RenderEngine/ObjLoader.h"
#include "../Config/PrefabManager.h"
#include "../Toolbox/Maths.h"

#include <iostream>

// ---------------------------------------------------------------------------
// init — scan PrefabManager for instanced prefabs and load their models
// ---------------------------------------------------------------------------

void InstancedModelManager::init(Loader* loader) {
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

        // Load the OBJ mesh data.
        ModelData meshData = OBJLoader::loadObjModel(objFile);
        if (meshData.getIndices().empty()) {
            std::cerr << "[InstancedModelManager] Failed to load OBJ '"
                      << objFile << "' for prefab '" << id << "'.\n";
            continue;
        }

        // Upload to GPU.
        RawModel* rawModel = loader->loadToVAO(meshData);
        GLuint texId = 0;
        if (!textureFile.empty()) {
            auto* tex = loader->loadTexture(textureFile);
            if (tex) texId = tex->getId();
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
                  << alias << "' (obj=" << objFile << ")\n";
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
    if (it == buckets_.end()) return;
    it->second.chunkInstances[chunkKey].push_back(transform);
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
}

// src/RenderEngine/InstancedModelManager.cpp

#include "InstancedModelManager.h"
#include "MasterRenderer.h"
#include "../RenderEngine/Loader.h"
#include "../RenderEngine/ObjLoader.h"
#include "../Config/PrefabManager.h"
#include "../Toolbox/Maths.h"

#include <algorithm>
#include <iostream>

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

        // Animated prefabs (animated: true or mesh path present) are rendered by
        // AnimatedRenderer / AnimationSystem, not by InstancedModelManager.
        // Skip them so we don't emit a spurious warning for animated characters
        // that correctly omit a static-OBJ "model" block.
        if (prefab.value("animated", false) || prefab.contains("mesh")) continue;

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

        // Cache a trimmed mesh AABB for the editor tile footprint.
        // Skip if SceneLoaderJson already set it (only set if not yet present).
        if (PrefabManager::get().getMeshHalfExtentsXZ(alias, 1.0f).x <= 0.0f) {
            const auto& verts = meshData.getVertices(); // [x, y, z, …]
            const size_t nVerts = verts.size() / 3;
            if (nVerts >= 10) {
                std::vector<float> xs, zs;
                xs.reserve(nVerts);
                zs.reserve(nVerts);
                for (size_t vi = 0; vi < nVerts; ++vi) {
                    xs.push_back(verts[vi * 3]);
                    zs.push_back(verts[vi * 3 + 2]);
                }
                std::sort(xs.begin(), xs.end());
                std::sort(zs.begin(), zs.end());

                const size_t trim = nVerts / 10;
                const float xMin = xs[trim],     xMax = xs[nVerts - 1 - trim];
                const float zMin = zs[trim],     zMax = zs[nVerts - 1 - trim];
                const float hx = (xMax - xMin) * 0.5f;
                const float hz = (zMax - zMin) * 0.5f;

                if (hx > 0.0f && hz > 0.0f) {
                    PrefabManager::get().setMeshAABB(alias,
                        glm::vec3(-hx, meshData.getMin().y,  hz),
                        glm::vec3( hx, meshData.getMax().y, -hz));
                }
            }
        }
    }

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
    initialized_ = false;
}
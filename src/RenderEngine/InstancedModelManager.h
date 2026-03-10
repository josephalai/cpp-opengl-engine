// src/RenderEngine/InstancedModelManager.h
//
// Data-driven instanced rendering manager (GEA Phase 5.4, Step 2).
//
// At startup, scans PrefabManager for every prefab with "render_mode": "instanced"
// and creates an empty InstancedModel bucket mapped to its alias.  During runtime,
// the ChunkManager pushes transform matrices into the appropriate bucket via
// addInstance().  Matrices are grouped by chunk key (int64_t) so that chunk
// unloads instantly purge all visuals for a given chunk.
//
// The engine does NOT need to know what a "tree" or "boulder" is — it only
// processes data-driven instanced batches.

#ifndef ENGINE_INSTANCED_MODEL_MANAGER_H
#define ENGINE_INSTANCED_MODEL_MANAGER_H

#include "InstancedModel.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <glm/glm.hpp>

class Loader;
class MasterRenderer;

class InstancedModelManager {
public:
    /// Scan PrefabManager for every prefab flagged "render_mode": "instanced",
    /// load its OBJ model + texture via the Loader, create an InstancedModel
    /// with an empty VBO, and store it in the internal map.
    void init(Loader* loader);

    /// Push a transform matrix for a given prefab alias into the bucket
    /// associated with the specified chunk key.
    void addInstance(const std::string& alias, int64_t chunkKey,
                     const glm::mat4& transform);

    /// Remove all instance matrices associated with a given chunk key.
    void removeChunk(int64_t chunkKey);

    /// Return true if the given alias has a registered instanced bucket.
    bool hasAlias(const std::string& alias) const;

    /// Submit all instanced batches to the MasterRenderer for rendering.
    /// This aggregates transforms across all loaded chunks for each model.
    void submitToRenderer(MasterRenderer* renderer);

    /// Clean up GPU resources.
    void cleanup();

private:
    struct Bucket {
        InstancedModel* model = nullptr;
        /// Per-chunk instance matrices.
        std::unordered_map<int64_t, std::vector<glm::mat4>> chunkInstances;
    };

    std::unordered_map<std::string, Bucket> buckets_;
    bool initialized_ = false;
};

#endif // ENGINE_INSTANCED_MODEL_MANAGER_H

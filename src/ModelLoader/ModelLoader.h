// src/ModelLoader/ModelLoader.h
// Facade: detects format by file extension and delegates to the right loader.

#ifndef ENGINE_MODELLOADER_H
#define ENGINE_MODELLOADER_H

#include <string>
#include "../Animation/AnimatedModel.h"
#include "../ModelLoader/GLTFLoader.h"
#include "../ModelLoader/ModelCache.h"

class ModelLoader {
public:
    /// Load any supported model format (OBJ via Assimp, FBX, glTF 2.0, GLB).
    /// Uses an internal cache to avoid redundant loads.
    /// Returns nullptr on failure.
    static AnimatedModel* load(const std::string& path) {
        return cache.getModel(path);
    }

    /// Load as a glTF asset (PBR material data included).
    static GLTFAsset* loadGLTF(const std::string& path) {
        return GLTFLoader::load(path);
    }

    /// Pre-load a batch of files.
    static void preload(const std::vector<std::string>& paths) {
        cache.preload(paths);
    }

    /// Release all cached models.
    static void clear() { cache.clear(); }

private:
    static ModelCache cache;
};

#endif // ENGINE_MODELLOADER_H

// src/Animation/AnimationLoader.h
// Loads AnimatedModel (mesh + skeleton + animation clips) from FBX/glTF via Assimp.

#ifndef ENGINE_ANIMATIONLOADER_H
#define ENGINE_ANIMATIONLOADER_H

#include <string>
#include "AnimatedModel.h"

class AnimationLoader {
public:
    /// Load a skeletal model from path (absolute or relative to Resources/Models/).
    /// Returns nullptr on failure.
    static AnimatedModel* load(const std::string& path);
};

#endif // ENGINE_ANIMATIONLOADER_H

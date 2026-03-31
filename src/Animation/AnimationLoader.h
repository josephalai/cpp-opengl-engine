// src/Animation/AnimationLoader.h
// Loads AnimatedModel (mesh + skeleton + animation clips) from FBX/glTF via Assimp.
//
// Three loading modes are supported:
//   1. load()                  — Monolithic: mesh + skeleton + embedded animations (legacy).
//   2. loadSkin()              — Modular: mesh + skeleton only; ignores mAnimations.
//   3. loadExternalAnimation() — Modular: animation-only glb stitched onto an existing Skeleton.

#ifndef ENGINE_ANIMATIONLOADER_H
#define ENGINE_ANIMATIONLOADER_H

#include <memory>
#include <string>
#include <vector>
#include "AnimatedModel.h"
#include "AnimationClip.h"
#include "Skeleton.h"

class AnimationLoader {
public:
    /// Load a skeletal model from path (absolute or relative to Resources/Models/).
    /// Returns nullptr on failure.
    /// Loads mesh, skeleton, AND all embedded animation clips (legacy/monolithic mode).
    static AnimatedModel* load(const std::string& path);

    /// Load a skin-only glb: extracts mesh geometry and the bind-pose bone hierarchy,
    /// but intentionally ignores any mAnimations present in the file.
    /// Use this when animations live in separate files (modular/MMO pipeline).
    /// Returns nullptr on failure.
    static AnimatedModel* loadSkin(const std::string& skinPath);

    /// Load a modular equipment/body part from a GLB file and remap its vertex
    /// bone indices to the master skeleton.  Returns ready-to-render sub-meshes
    /// whose boneIDs reference bones in @p masterSkeleton (by name matching).
    /// The returned meshes already have their OpenGL VAO/VBO/EBO set up.
    static std::vector<AnimatedMesh> loadModularPart(
        const std::string& path,
        const Skeleton& masterSkeleton);

    /// Load an animation-only glb and map its channels onto an existing Skeleton.
    /// The channel names in the file (e.g. "mixamorig:RightArm") are matched by name
    /// to bones in targetSkeleton.  Channels that do not match any bone are silently
    /// skipped so retargeting between similarly-named rigs is possible.
    /// Returns nullptr on failure or if no animations were found in the file.
    static std::shared_ptr<AnimationClip> loadExternalAnimation(
        const std::string& animPath,
        Skeleton* targetSkeleton,
        const std::string& expectedName = "");
};

#endif // ENGINE_ANIMATIONLOADER_H

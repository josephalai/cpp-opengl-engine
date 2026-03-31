// src/Animation/ModularMeshPart.h
// Represents a single modular body/equipment part (e.g. Head, Torso armour).
// Each part owns one or more AnimatedMesh sub-meshes whose bone indices have
// been remapped to the master skeleton at load time.

#ifndef ENGINE_MODULARMESHPART_H
#define ENGINE_MODULARMESHPART_H

#include <string>
#include <vector>
#include "AnimatedModel.h"
#include "EquipmentSlot.h"

struct ModularMeshPart {
    std::string                assetPath;         ///< Source GLB path.
    EquipmentSlot              slot = EquipmentSlot::Count;
    std::vector<AnimatedMesh>  meshes;            ///< Bone-remapped sub-meshes.
    bool                       hidesNakedPart = true;

    /// Release OpenGL resources for every sub-mesh owned by this part.
    void cleanUp() {
        for (auto& m : meshes) {
            if (m.textureID) { glDeleteTextures(1, &m.textureID); m.textureID = 0; }
            if (m.VAO) { glDeleteVertexArrays(1, &m.VAO); m.VAO = 0; }
            if (m.VBO) { glDeleteBuffers(1, &m.VBO);      m.VBO = 0; }
            if (m.EBO) { glDeleteBuffers(1, &m.EBO);      m.EBO = 0; }
        }
    }
};

#endif // ENGINE_MODULARMESHPART_H

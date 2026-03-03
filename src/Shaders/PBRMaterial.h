// PBRMaterial.h — owns a PBRShader reference and manages texture slots / fallback values.

#ifndef ENGINE_PBRMATERIAL_H
#define ENGINE_PBRMATERIAL_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include "../Shaders/PBRShader.h"

/// Describes a PBR metallic-roughness material.
/// Textures are optional; constant values are used as fallback.
struct PBRMaterial {
    // Owning shader (shared across all PBR materials)
    PBRShader* shader = nullptr;

    // --- Texture IDs (0 = not provided) ---
    GLuint albedoTexture    = 0;
    GLuint normalTexture    = 0;
    GLuint metallicTexture  = 0;
    GLuint roughnessTexture = 0;
    GLuint aoTexture        = 0;

    // --- Fallback constant values ---
    glm::vec3 albedoValue    = glm::vec3(1.0f);  ///< white albedo
    float     metallicValue  = 0.0f;
    float     roughnessValue = 0.5f;
    float     aoValue        = 1.0f;

    /// Bind all textures and set shader uniforms.
    /// The shader must already be started (shader->start()) before calling this.
    void bind() const {
        if (!shader) return;

        bool useAlbedo    = (albedoTexture    != 0);
        bool useNormal    = (normalTexture    != 0);
        bool useMetallic  = (metallicTexture  != 0);
        bool useRoughness = (roughnessTexture != 0);
        bool useAO        = (aoTexture        != 0);

        shader->loadUseAlbedoMap(useAlbedo);
        shader->loadUseNormalMap(useNormal);
        shader->loadUseMetallicMap(useMetallic);
        shader->loadUseRoughnessMap(useRoughness);
        shader->loadUseAOMap(useAO);

        // Always load fallback values (used when the map flag is false)
        shader->loadAlbedoValue(albedoValue);
        shader->loadMetallicValue(metallicValue);
        shader->loadRoughnessValue(roughnessValue);
        shader->loadAOValue(aoValue);

        // Bind textures to slots 0-4
        auto bindTex = [](GLuint slot, GLuint texId) {
            if (texId != 0) {
                glActiveTexture(GL_TEXTURE0 + slot);
                glBindTexture(GL_TEXTURE_2D, texId);
            }
        };
        bindTex(0, albedoTexture);
        bindTex(1, normalTexture);
        bindTex(2, metallicTexture);
        bindTex(3, roughnessTexture);
        bindTex(4, aoTexture);

        shader->connectTextureUnits();
    }

    /// Unbind textures.
    void unbind() const {
        for (int i = 0; i < 5; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
};

#endif // ENGINE_PBRMATERIAL_H

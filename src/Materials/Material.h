// src/Materials/Material.h
// Rendering material — encapsulates shader uniforms (texture IDs, surface properties).
// Decouples the rendering material concept from raw model data (ModelTexture),
// enabling material sharing across models and runtime material swapping.
//
// PBR-ready: albedo, normal map, shininess, reflectivity, metallic, roughness, ao.

#ifndef ENGINE_MATERIAL_H
#define ENGINE_MATERIAL_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

namespace Materials {

class Material {
public:
    Material() = default;

    // --- Texture IDs (0 = not set) ---
    void setAlbedoTexture(GLuint texId)   { albedoTex_   = texId; }
    void setNormalMap    (GLuint texId)   { normalMap_   = texId; }

    GLuint getAlbedoTexture() const { return albedoTex_; }
    GLuint getNormalMap()     const { return normalMap_; }

    // --- Surface properties ---
    void  setShininess   (float s)  { shininess_    = s; }
    void  setReflectivity(float r)  { reflectivity_ = r; }
    void  setMetallic    (float m)  { metallic_     = m; }
    void  setRoughness   (float r)  { roughness_    = r; }
    void  setAmbientOcclusion(float a) { ao_         = a; }

    float getShininess()        const { return shininess_; }
    float getReflectivity()     const { return reflectivity_; }
    float getMetallic()         const { return metallic_; }
    float getRoughness()        const { return roughness_; }
    float getAmbientOcclusion() const { return ao_; }

    /// Bind material textures to GL texture units.
    /// Call between shader->start() and the draw call.
    void bind() const;

    /// Convenience builder — creates a simple Blinn-Phong material.
    static Material makeBlinnPhong(GLuint albedo, float shininess = 32.0f,
                                   float reflectivity = 0.5f);

private:
    GLuint albedoTex_   = 0;
    GLuint normalMap_   = 0;

    float shininess_    = 32.0f;
    float reflectivity_ = 0.5f;
    // PBR-ready extras
    float metallic_     = 0.0f;
    float roughness_    = 0.5f;
    float ao_           = 1.0f;
};

} // namespace Materials

#endif // ENGINE_MATERIAL_H

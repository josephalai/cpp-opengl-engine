// src/Materials/Material.cpp

#include "Material.h"

namespace Materials {

void Material::bind() const {
    // Bind albedo texture to unit 0
    if (albedoTex_) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, albedoTex_);
    }
    // Bind normal map to unit 1
    if (normalMap_) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normalMap_);
    }
    // Shader uniforms (shininess, reflectivity, etc.) are uploaded by
    // the calling renderer via its shader's load* methods.
}

Material Material::makeBlinnPhong(GLuint albedo, float shininess, float reflectivity) {
    Material m;
    m.setAlbedoTexture(albedo);
    m.setShininess(shininess);
    m.setReflectivity(reflectivity);
    return m;
}

} // namespace Materials

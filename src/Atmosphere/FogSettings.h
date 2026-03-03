// FogSettings.h — centralised fog/atmosphere configuration.
// Include this header wherever fog uniforms are loaded into shaders.

#ifndef ENGINE_FOGSETTINGS_H
#define ENGINE_FOGSETTINGS_H

#include <glm/glm.hpp>

/// Fog equation mode.
enum class FogMode {
    Linear,              ///< GL-style linear fog: factor = (end - dist) / (end - start)
    Exponential,         ///< factor = exp(-density * dist)
    ExponentialSquared   ///< factor = exp(-(density * dist)^2)
};

/// All fog / atmosphere parameters in one place.
struct FogSettings {
    // --- basic fog ---
    FogMode   mode      = FogMode::ExponentialSquared;
    float     density   = 0.007f;   ///< used for Exponential / ExponentialSquared modes
    float     start     = 10.0f;    ///< used for Linear mode only
    float     end       = 800.0f;   ///< used for Linear mode only
    glm::vec3 color     = glm::vec3(0.5f, 0.6f, 0.7f); ///< should match sky/skybox color

    // --- height fog ---
    bool  heightFogEnabled = false;
    float heightFogStart   = 0.0f;   ///< world-space Y below which fog thickens
    float heightFogEnd     = -20.0f; ///< world-space Y at which fog is fully opaque

    // --- atmospheric scattering (simplified) ---
    bool      scatteringEnabled = false;
    glm::vec3 sunDirection      = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
    glm::vec3 sunColor          = glm::vec3(1.0f, 0.9f, 0.7f);
    float     scatterStrength   = 0.4f;

    /// Compute linear fog visibility for a given distance.
    float linearVisibility(float dist) const {
        if (dist >= end)   return 0.0f;
        if (dist <= start) return 1.0f;
        return (end - dist) / (end - start);
    }

    /// Compute exponential-squared visibility.
    float expSqVisibility(float dist) const {
        float f = density * dist;
        return glm::clamp(glm::exp(-f * f), 0.0f, 1.0f);
    }
};

#endif // ENGINE_FOGSETTINGS_H

#version 410 core
// Epic Water Fragment Shader (Failsafe Version)

in vec4 clipSpace;
in vec2 texCoords;
in vec3 toCameraVector;
in vec3 fromLightVector;

out vec4 out_color;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform sampler2D depthMap;

uniform float moveFactor; 
uniform vec3  lightColor;

// --- Tweakable "Epic" Constants ---
const float kWaveStrength   = 0.04;
const float kShineDamper    = 80.0;     
const float kReflectivity   = 0.5;
const vec3  kWaterTint      = vec3(0.0, 0.3, 0.5); // Rich ocean blue

void main() {
    // 1. Normalized Device Coordinates -> Screen-space UV
    vec2 ndc = (clipSpace.xy / clipSpace.w) * 0.5 + 0.5;
    
    // Flipped Y for reflection (Safely clamped for OpenGL FBOs)
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);

    // 2. SAFE WATER DEPTH CALCULATION
    float near = 0.1;
    float far  = 1000.0;
    
    // Read the terrain depth from the FBO
    float depth = texture(depthMap, refractTexCoords).r;
    
    float floorDistance = 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
    float waterDistance = 2.0 * near * far / (far + near - (2.0 * gl_FragCoord.z - 1.0) * (far - near));
    float waterDepth = floorDistance - waterDistance;

    // FAILSAFE: If the depth buffer is empty/missing on this driver, depth returns 1.0.
    // This makes waterDepth massive/negative. We catch this and force a safe median depth.
    if (depth > 0.9999 || depth < 0.0001) {
        waterDepth = 50.0; 
    }

    // 3. CROSS-HATCHED DUDV DISTORTION (organic waves)
    vec2 distortedUV1 = texture(dudvMap, vec2(texCoords.x + moveFactor, texCoords.y)).rg * 0.1;
    distortedUV1 = texCoords + vec2(distortedUV1.x, distortedUV1.y + moveFactor);
    vec2 totalDistortion1 = (texture(dudvMap, distortedUV1).rg * 2.0 - 1.0) * kWaveStrength;

    vec2 distortedUV2 = texture(dudvMap, vec2(-texCoords.x + moveFactor, texCoords.y + moveFactor)).rg * 0.1;
    distortedUV2 = -texCoords + vec2(distortedUV2.x, distortedUV2.y - moveFactor);
    vec2 totalDistortion2 = (texture(dudvMap, distortedUV2).rg * 2.0 - 1.0) * kWaveStrength;

    vec2 totalDistortion = totalDistortion1 + totalDistortion2;

    // Dampen distortion near the shore to prevent bleeding artifacts
    totalDistortion *= clamp(waterDepth / 20.0, 0.0, 1.0);

    reflectTexCoords += totalDistortion;
    reflectTexCoords.x = clamp(reflectTexCoords.x, 0.001, 0.999);
    reflectTexCoords.y = clamp(reflectTexCoords.y, 0.001, 0.999); // Safe clamping

    refractTexCoords += totalDistortion;
    refractTexCoords  = clamp(refractTexCoords, 0.001, 0.999);

    // 4. SAMPLE ENVIRONMENT
    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);

    // 5. VOLUMETRIC MURKINESS (Tint refraction based on depth)
    // Max out at 0.7 opacity so we can ALWAYS see the refraction underneath!
    float murkiness = clamp(waterDepth / 40.0, 0.0, 0.7);
    refractColor = mix(refractColor, vec4(kWaterTint, 1.0), murkiness);

    // 6. NORMALS (Cross-hatched for complexity)
    vec4 normalMapColor1 = texture(normalMap, distortedUV1);
    vec4 normalMapColor2 = texture(normalMap, distortedUV2);
    vec3 normal1 = vec3(normalMapColor1.r * 2.0 - 1.0, normalMapColor1.b * 3.0, normalMapColor1.g * 2.0 - 1.0);
    vec3 normal2 = vec3(normalMapColor2.r * 2.0 - 1.0, normalMapColor2.b * 3.0, normalMapColor2.g * 2.0 - 1.0);
    vec3 normal = normalize(normal1 + normal2);

    // 7. FRESNEL EFFECT
    vec3 viewVector = normalize(toCameraVector);
    float refractiveFactor = dot(viewVector, normal);
    refractiveFactor = pow(clamp(refractiveFactor, 0.0, 1.0), 2.0);
    // Don't let it become a 100% perfect mirror, keep it slightly water-like
    refractiveFactor = clamp(refractiveFactor, 0.15, 0.85);

    // 8. SPECULAR HIGHLIGHTS (Sun Glint)
    vec3 reflectedLight = reflect(normalize(fromLightVector), normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, kShineDamper);
    vec3 specularHighlights = lightColor * specular * kReflectivity * clamp(waterDepth / 5.0, 0.0, 1.0);

    // 9. FINAL MIX
    out_color = mix(reflectColor, refractColor, refractiveFactor);
    out_color = out_color + vec4(specularHighlights, 0.0);

    // 10. SOFT EDGES
    // Fade out alpha perfectly where the water touches the terrain geometry
    out_color.a = clamp(waterDepth / 5.0, 0.0, 1.0);
}

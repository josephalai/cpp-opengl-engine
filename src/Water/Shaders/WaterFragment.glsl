#version 410 core
// Water fragment shader with DuDv distortion, Fresnel effect, specular highlights

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

uniform float moveFactor;  // animated wave offset
uniform vec3  lightColor;

const float kWaveStrength   = 0.02;
const float kShineDamper    = 20.0;
const float kReflectivity   = 0.5;

void main() {
    // Normalized device coords → screen-space UV for reflection/refraction lookup
    vec2 ndc = (clipSpace.xy / clipSpace.w) * 0.5 + 0.5;
    vec2 reflectTexCoords = vec2(ndc.x, -ndc.y);
    vec2 refractTexCoords = vec2(ndc.x,  ndc.y);

    // DuDv distortion
    vec2 distortedUV = texture(dudvMap, vec2(texCoords.x + moveFactor, texCoords.y)).rg * 0.1;
    distortedUV      = texCoords + vec2(distortedUV.x, distortedUV.y + moveFactor);
    vec2 totalDistortion = (texture(dudvMap, distortedUV).rg * 2.0 - 1.0) * kWaveStrength;

    reflectTexCoords += totalDistortion;
    reflectTexCoords.x = clamp(reflectTexCoords.x, 0.001, 0.999);
    reflectTexCoords.y = clamp(reflectTexCoords.y, -0.999, -0.001);

    refractTexCoords += totalDistortion;
    refractTexCoords  = clamp(refractTexCoords, 0.001, 0.999);

    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);

    // Normal map
    vec4 normalMapColor = texture(normalMap, distortedUV);
    vec3 normal = vec3(normalMapColor.r * 2.0 - 1.0, normalMapColor.b * 3.0, normalMapColor.g * 2.0 - 1.0);
    normal = normalize(normal);

    // Fresnel
    vec3 viewVector = normalize(toCameraVector);
    float refractiveFactor = dot(viewVector, normal);
    refractiveFactor = pow(refractiveFactor, 0.5);
    refractiveFactor = clamp(refractiveFactor, 0.0, 1.0);

    // Specular highlight
    vec3 reflectedLight = reflect(normalize(fromLightVector), normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, kShineDamper);
    vec3 specularHighlights = lightColor * specular * kReflectivity;

    out_color = mix(reflectColor, refractColor, refractiveFactor);
    out_color = mix(out_color, vec4(0.0, 0.3, 0.5, 1.0), 0.2) + vec4(specularHighlights, 0.0);
    out_color.a = 0.8;
}

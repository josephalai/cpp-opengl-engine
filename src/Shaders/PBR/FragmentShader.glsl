#version 410 core
// PBR Fragment Shader — Cook-Torrance metallic-roughness BRDF

const float PI = 3.14159265359;

struct Light {
    vec3 position;
    vec3 diffuse;
    vec3 ambient;
    vec3 specular;
    float constant;   // -1.0 → directional
    float linear;
    float quadratic;
};
uniform Light light[4];

// PBR material textures/values
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

uniform bool  useAlbedoMap;
uniform bool  useNormalMap;
uniform bool  useMetallicMap;
uniform bool  useRoughnessMap;
uniform bool  useAOMap;

uniform vec3  albedoValue;
uniform float metallicValue;
uniform float roughnessValue;
uniform float aoValue;

uniform vec3 viewPosition;
uniform vec3 skyColor;

in vec2  pass_texCoords;
in vec3  fragPos_world;
in vec3  pass_normal;
in mat3  TBN;
in vec3  toLightVector[4];
in float visibility;

out vec4 out_color;

// ----------------------------------------------------------------------------
// Distribution function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float denom  = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// Geometry function (Smith's Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// Fresnel (Schlick approximation)
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------------------------------------------------------
void main() {
    // --- Sample material properties ---
    vec3  albedo    = useAlbedoMap    ? pow(texture(albedoMap,    pass_texCoords).rgb, vec3(2.2)) : albedoValue;
    float metallic  = useMetallicMap  ? texture(metallicMap,  pass_texCoords).r : metallicValue;
    float roughness = useRoughnessMap ? texture(roughnessMap, pass_texCoords).r : roughnessValue;
    float ao        = useAOMap        ? texture(aoMap,        pass_texCoords).r : aoValue;

    // --- Normal ---
    vec3 N;
    if (useNormalMap) {
        vec3 n = texture(normalMap, pass_texCoords).rgb * 2.0 - 1.0;
        N = normalize(TBN * n);
    } else {
        N = normalize(pass_normal);
    }

    vec3 V = normalize(viewPosition - fragPos_world);

    // F0 — base reflectance (0.04 for dielectrics, albedo for metals)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // --- Accumulate radiance from each light ---
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        vec3 L;
        float attenuation = 1.0;

        if (light[i].constant < 0.0) {
            // Directional light
            L = normalize(-light[i].position);
        } else {
            L = normalize(toLightVector[i]);
            float dist  = length(toLightVector[i]);
            attenuation = 1.0 / (light[i].constant
                                 + light[i].linear    * dist
                                 + light[i].quadratic * dist * dist);
        }

        vec3 H        = normalize(V + L);
        vec3 radiance = light[i].diffuse * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color   = ambient + Lo;

    // HDR tone-mapping + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    vec4 result = vec4(color, 1.0);
    out_color = mix(vec4(skyColor, 1.0), result, visibility);
}

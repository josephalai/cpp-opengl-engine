#version 410 core
// PBR Vertex Shader — Cook-Torrance BRDF pipeline

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

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 textureCoords;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;

out vec2  pass_texCoords;
out vec3  fragPos_world;
out vec3  pass_normal;
out mat3  TBN;
out vec3  toLightVector[4];
out float visibility;

uniform mat4 transformationMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

// Fog
uniform float fogDensity;

void main() {
    vec4 worldPos          = transformationMatrix * vec4(position, 1.0);
    fragPos_world          = worldPos.xyz;
    vec4 viewPos           = viewMatrix * worldPos;
    gl_Position            = projectionMatrix * viewPos;
    pass_texCoords         = textureCoords;

    // TBN matrix for normal mapping
    mat3 normalMatrix = transpose(inverse(mat3(transformationMatrix)));
    vec3 N = normalize(normalMatrix * normal);
    vec3 T = normalize(normalMatrix * tangent);
    T = normalize(T - dot(T, N) * N);   // re-orthogonalise
    vec3 B = cross(N, T);
    TBN         = mat3(T, B, N);
    pass_normal = N;

    for (int i = 0; i < 4; i++) {
        toLightVector[i] = light[i].position - worldPos.xyz;
    }

    // Exponential-squared fog
    float dist = length(viewPos.xyz);
    visibility = exp(-pow(dist * fogDensity, 2.0));
    visibility = clamp(visibility, 0.0, 1.0);
}

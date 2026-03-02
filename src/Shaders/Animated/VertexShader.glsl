#version 410 core

// Skeletal animation vertex shader.
// Supports up to MAX_BONES bone influences per vertex (max 4).

#define MAX_BONES 100

struct Light {
    vec3  position;
    vec3  diffuse;
    vec3  ambient;
    vec3  specular;
    float constant;
    float linear;
    float quadratic;
};

uniform Light light[4];

layout(location = 0) in vec3  position;
layout(location = 1) in vec3  normal;
layout(location = 2) in vec2  textureCoords;
layout(location = 3) in ivec4 boneIDs;
layout(location = 4) in vec4  boneWeights;

out vec2 pass_textureCoords;
out vec3 surfaceNormal;
out vec3 toLightVector[4];
out vec4 worldPosition;
out float visibility;

uniform mat4 transformationMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 boneMatrices[MAX_BONES];

const float density  = 0.007;
const float gradient = 1.5;

void main() {
    // Accumulate skinned position and normal
    vec4 skinnedPos    = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        float w = boneWeights[i];
        if (w <= 0.0) continue;
        int boneID = boneIDs[i];
        if (boneID < 0 || boneID >= MAX_BONES) continue;
        mat4 bm = boneMatrices[boneID];
        skinnedPos    += w * (bm * vec4(position, 1.0));
        skinnedNormal += w * mat3(transpose(inverse(bm))) * normal;
    }

    // Fallback: if no valid bones influence this vertex, use identity (pass-through)
    if (dot(boneWeights, boneWeights) < 0.0001) {
        skinnedPos    = vec4(position, 1.0);
        skinnedNormal = normal;
    }

    worldPosition = transformationMatrix * skinnedPos;
    vec4 posRelativeToCam = viewMatrix * worldPosition;
    gl_Position = projectionMatrix * posRelativeToCam;

    pass_textureCoords = textureCoords;
    surfaceNormal = mat3(transpose(inverse(transformationMatrix))) * normalize(skinnedNormal);

    for (int i = 0; i < 4; ++i) {
        toLightVector[i] = light[i].position - worldPosition.xyz;
    }

    float distance = length(posRelativeToCam.xyz);
    visibility = clamp(exp(-pow(distance * density, gradient)), 0.0, 1.0);
}

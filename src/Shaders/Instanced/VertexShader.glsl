#version 410 core

// Instanced rendering vertex shader.
// Reads the model matrix from per-instance attributes (locations 3-6).

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

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 textureCoords;
layout(location = 2) in vec3 normal;
// mat4 occupies 4 consecutive vec4 attribute locations
layout(location = 3) in vec4 instanceMatrix0;
layout(location = 4) in vec4 instanceMatrix1;
layout(location = 5) in vec4 instanceMatrix2;
layout(location = 6) in vec4 instanceMatrix3;

out vec2  pass_textureCoords;
out vec3  surfaceNormal;
out vec3  toLightVector[4];
out vec4  worldPosition;
out float visibility;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

const float density  = 0.007;
const float gradient = 1.5;

void main() {
    mat4 instanceMatrix = mat4(instanceMatrix0,
                               instanceMatrix1,
                               instanceMatrix2,
                               instanceMatrix3);

    worldPosition = instanceMatrix * vec4(position, 1.0);
    vec4 posRelCam = viewMatrix * worldPosition;
    gl_Position    = projectionMatrix * posRelCam;

    pass_textureCoords = textureCoords;
    surfaceNormal = mat3(transpose(inverse(instanceMatrix))) * normal;

    for (int i = 0; i < 4; ++i) {
        toLightVector[i] = light[i].position - worldPosition.xyz;
    }

    float dist = length(posRelCam.xyz);
    visibility = clamp(exp(-pow(dist * density, gradient)), 0.0, 1.0);
}

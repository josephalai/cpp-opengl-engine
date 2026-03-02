#version 410 core
// Water vertex shader

layout (location = 0) in vec2 position;

out vec4 clipSpace;
out vec2 texCoords;
out vec3 toCameraVector;
out vec3 fromLightVector;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformationMatrix;
uniform vec3 cameraPosition;
uniform vec3 lightPosition;

const float kTilingFactor = 6.0;

void main() {
    vec4 worldPosition = transformationMatrix * vec4(position.x, 0.0, position.y, 1.0);
    clipSpace          = projectionMatrix * viewMatrix * worldPosition;
    gl_Position        = clipSpace;
    texCoords          = vec2(position.x / 2.0 + 0.5, position.y / 2.0 + 0.5) * kTilingFactor;
    toCameraVector     = cameraPosition - worldPosition.xyz;
    fromLightVector    = worldPosition.xyz - lightPosition;
}

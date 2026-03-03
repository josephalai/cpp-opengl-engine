#version 410 core
// Shadow map depth-only vertex shader

layout (location = 0) in vec3 position;

uniform mat4 lightSpaceMatrix;
uniform mat4 transformationMatrix;

void main() {
    gl_Position = lightSpaceMatrix * transformationMatrix * vec4(position, 1.0);
}

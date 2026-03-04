#version 330 core

in vec3 position;
in vec3 color;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

out vec3 fragColor;

void main(void) {
    gl_Position = projectionMatrix * viewMatrix * vec4(position, 1.0);
    fragColor = color;
}

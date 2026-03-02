#version 330 core

in vec2 textureCoords;

out vec4 out_Color;

uniform vec3 color;
uniform float alpha;

void main(void){
    out_Color = vec4(color.xyz, alpha);
}
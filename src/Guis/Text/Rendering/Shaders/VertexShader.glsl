#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 transformationMatrix;

void main()
{
    mat4 transform = mat4(0.50f, 0.0f, 0.0f, 0.0f, 0.0f, 0.50f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    gl_Position =  projection * transformationMatrix * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
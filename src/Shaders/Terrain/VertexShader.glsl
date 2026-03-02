#version 330 core
struct Light {
    vec3 position;

    vec3 diffuse;
    vec3 ambient;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};
uniform Light light[4];

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 textureCoords;
layout (location = 2) in vec3 normal;

out vec4 worldPosition;
out vec3 surfaceNormal;
out vec2 pass_textureCoords;
out vec3 toLightVector[4];

out float visibility;

uniform mat4 transformationMatrix; // model matrix
uniform mat4 viewMatrix; // view matrix
uniform mat4 projectionMatrix; // projection matrix

out vec3 toCameraVector;

uniform float fogDensity;  // configurable fog density

const float gradient = 5.0;

void main()
{
    worldPosition = transformationMatrix * vec4(position, 1.0);

    vec4 positionRelativeToCam = viewMatrix * worldPosition;

    gl_Position = projectionMatrix * positionRelativeToCam;
    pass_textureCoords = textureCoords;

    surfaceNormal = mat3(transpose(inverse(transformationMatrix))) * normal;
    for (int i = 0; i < 4; i++) {
        toLightVector[i] = light[i].position - worldPosition.xyz;
    }

    toCameraVector = (inverse(viewMatrix) * vec4(0.0,0.0,0.0,1.0)).xyz - worldPosition.xyz;

    float distance = length(positionRelativeToCam.xyz);
    visibility = exp(-pow((distance * fogDensity), gradient));
    visibility = clamp(visibility, 0.0, 1.0);
}

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
layout (location = 3) in vec3 tangent;   // optional — used for normal mapping

out vec2 pass_textureCoords;
out vec3 surfaceNormal;
out vec3 toLightVector[4];
out vec4 worldPosition;
out float visibility;
out mat3 TBN;   // tangent-space basis (for normal mapping)

uniform mat4 transformationMatrix; // model matrix
uniform mat4 viewMatrix; // view matrix
uniform mat4 projectionMatrix; // projection matrix

uniform float useFakeLighting;

uniform float numberOfRows;
uniform vec2 offset;

uniform float fogDensity;  // replaces hard-coded density

const float gradient = 5.0;


void main()
{
    worldPosition = transformationMatrix * vec4(position, 1.0);
    vec4 positionRelativeToCam = viewMatrix * worldPosition;
    gl_Position = projectionMatrix * positionRelativeToCam;
    pass_textureCoords = (textureCoords / numberOfRows) + offset;

    mat3 normalMatrix = transpose(inverse(mat3(transformationMatrix)));

    vec3 actualNormal = normal;
    if (useFakeLighting > 0.5) {
        actualNormal = vec3(0.0, 1, 0.0);
    }

    surfaceNormal = normalMatrix * actualNormal;

    // Build TBN matrix for normal mapping
    vec3 N = normalize(normalMatrix * normal);
    vec3 T = normalize(normalMatrix * tangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);

    for (int i = 0; i < 4; i++) {
        toLightVector[i] = light[i].position - worldPosition.xyz;
    }

    float distance = length(positionRelativeToCam.xyz);
    visibility = exp(-pow((distance * fogDensity), gradient));
    visibility = clamp(visibility, 0.0, 1.0);
}

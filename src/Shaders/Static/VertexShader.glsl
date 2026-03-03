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

// Instanced rendering: per-instance model matrix supplied as 4 consecutive vec4
// attributes (locations 4–7).  Only read when useInstancing == true.
layout (location = 4) in vec4 instanceMatrix0;
layout (location = 5) in vec4 instanceMatrix1;
layout (location = 6) in vec4 instanceMatrix2;
layout (location = 7) in vec4 instanceMatrix3;

out vec2 pass_textureCoords;
out vec3 surfaceNormal;
out vec3 toLightVector[4];
out vec4 worldPosition;
out float visibility;
out mat3 TBN;   // tangent-space basis (for normal mapping)

uniform mat4 transformationMatrix; // model matrix (used when useInstancing == false)
uniform mat4 viewMatrix; // view matrix
uniform mat4 projectionMatrix; // projection matrix

uniform bool useInstancing;  // true → read model matrix from instance attributes

uniform float useFakeLighting;

uniform float numberOfRows;
uniform vec2 offset;

uniform float fogDensity;  // replaces hard-coded density

const float gradient = 5.0;


void main()
{
    // Select model matrix: per-instance attribute or per-draw uniform
    mat4 modelMatrix;
    if (useInstancing) {
        modelMatrix = mat4(instanceMatrix0, instanceMatrix1,
                           instanceMatrix2, instanceMatrix3);
    } else {
        modelMatrix = transformationMatrix;
    }

    worldPosition = modelMatrix * vec4(position, 1.0);
    vec4 positionRelativeToCam = viewMatrix * worldPosition;
    gl_Position = projectionMatrix * positionRelativeToCam;
    pass_textureCoords = (textureCoords / numberOfRows) + offset;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

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

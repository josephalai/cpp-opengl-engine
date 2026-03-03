#version 410 core

// Instanced rendering fragment shader — same lighting as StaticShader.

struct Material {
    float shininess;
    float reflectivity;
};
uniform Material material;

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

in vec2  pass_textureCoords;
in vec3  surfaceNormal;
in vec3  toLightVector[4];
in vec4  worldPosition;
in float visibility;

out vec4 out_color;

uniform sampler2D textureSampler;
uniform vec3      viewPosition;
uniform vec3      skyColor;

vec3 CalcPointLight(Light l, vec3 lightVec, vec3 n, vec3 toCam, vec4 col);
vec3 CalcDirLight  (Light l,                vec3 n, vec3 toCam, vec4 col);

void main() {
    vec4 texColor = texture(textureSampler, pass_textureCoords);
    if (texColor.a < 0.5) discard;

    vec3 n      = normalize(surfaceNormal);
    vec3 toCam  = normalize(viewPosition - worldPosition.xyz);
    vec3 result = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        if (light[i].constant > 0.0)
            result += CalcPointLight(light[i], toLightVector[i], n, toCam, texColor);
        else
            result += CalcDirLight(light[i], n, toCam, texColor);
    }

    out_color = mix(vec4(skyColor, 1.0), vec4(result, 1.0), visibility);
}

vec3 CalcPointLight(Light l, vec3 lightVec, vec3 n, vec3 toCam, vec4 col) {
    vec3  unitLight = normalize(lightVec);
    float diff      = max(dot(n, unitLight), 0.0);
    vec3  h         = normalize(unitLight + toCam);
    float spec      = pow(max(dot(n, h), 0.0), material.shininess);
    float dist      = length(lightVec);
    float atten     = 1.0 / (l.constant + l.linear * dist + l.quadratic * dist * dist);
    return (l.ambient * col.rgb + l.diffuse * diff * col.rgb + l.specular * spec * material.reflectivity) * atten;
}

vec3 CalcDirLight(Light l, vec3 n, vec3 toCam, vec4 col) {
    vec3  dir  = normalize(-l.position);
    float diff = max(dot(n, dir), 0.0);
    vec3  h    = normalize(dir + toCam);
    float spec = pow(max(dot(n, h), 0.0), material.shininess);
    return l.ambient * col.rgb
         + l.diffuse  * diff * col.rgb
         + l.specular * spec * material.reflectivity;
}

#version 410 core

// Skeletal animation fragment shader with Blinn-Phong lighting.

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

vec3 CalcPointLight(Light l, vec3 lightVec, vec3 unitNorm, vec3 toCam, vec4 color);
vec3 CalcDirLight  (Light l,                vec3 unitNorm, vec3 toCam, vec4 color);

void main() {
    vec4 texColor = texture(textureSampler, pass_textureCoords);
    if (texColor.a < 0.5) discard;

    vec3 unitNorm = normalize(surfaceNormal);
    vec3 toCam    = normalize(viewPosition - worldPosition.xyz);

    vec3 result = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        if (light[i].constant > 0.0)
            result += CalcPointLight(light[i], toLightVector[i], unitNorm, toCam, texColor);
        else
            result += CalcDirLight(light[i], unitNorm, toCam, texColor);
    }

    out_color = mix(vec4(skyColor, 1.0), vec4(result, 1.0), visibility);
}

vec3 CalcPointLight(Light l, vec3 lightVec, vec3 unitNorm, vec3 toCam, vec4 color) {
    vec3 unitLight = normalize(lightVec);
    float diff     = max(dot(unitNorm, unitLight), 0.0);
    vec3  half     = normalize(unitLight + toCam);
    float spec     = pow(max(dot(unitNorm, half), 0.0), material.shininess);
    float dist     = length(lightVec);
    float atten    = 1.0 / (l.constant + l.linear * dist + l.quadratic * dist * dist);
    vec3 ambient   = l.ambient  * color.rgb * atten;
    vec3 diffuse   = l.diffuse  * diff * color.rgb * atten;
    vec3 specular  = l.specular * spec * material.reflectivity * atten;
    return ambient + diffuse + specular;
}

vec3 CalcDirLight(Light l, vec3 unitNorm, vec3 toCam, vec4 color) {
    vec3 lightDir = normalize(-l.position);
    float diff    = max(dot(unitNorm, lightDir), 0.0);
    vec3  half    = normalize(lightDir + toCam);
    float spec    = pow(max(dot(unitNorm, half), 0.0), material.shininess);
    return l.ambient * color.rgb
         + l.diffuse * diff * color.rgb
         + l.specular * spec * material.reflectivity;
}

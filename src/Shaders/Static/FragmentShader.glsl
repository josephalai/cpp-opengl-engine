#version 330 core

struct Material {
    float shininess;
    float reflectivity;
};


uniform Material material;

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


in vec2 pass_textureCoords;
in vec3 surfaceNormal;
in vec3 toLightVector[4];

in vec4 worldPosition;
in float visibility;
in mat3 TBN;

out vec4 out_color;

uniform sampler2D textureSampler;
uniform sampler2D normalMapSampler;   // normal map (optional)
uniform sampler2D specularMapSampler; // specular map (optional)

uniform bool useNormalMap;
uniform bool useSpecularMap;

uniform vec3 lightColor;
uniform vec3 viewPosition;

uniform float reflectivity;
uniform float shineDamper;
uniform float ambientStrength;
uniform vec3 skyColor;


vec3 CalcPointLight(Light light, vec3 lightVector, vec3 unitNormal, vec3 unitVectorToCamera, vec4 totalColor);
vec3 CalcDirLight(Light light, vec3 unitNormal, vec3 unitVectorToCamera, vec4 totalColor);

void main() {
    vec4 textureColor = texture(textureSampler, pass_textureCoords);
    if (textureColor.a < 0.5) {
        discard;
    }

    vec3 unitNormal;
    if (useNormalMap) {
        vec3 n = texture(normalMapSampler, pass_textureCoords).rgb * 2.0 - 1.0;
        unitNormal = normalize(TBN * n);
    } else {
        unitNormal = normalize(surfaceNormal);
    }

    vec3 unitVectorToCamera = normalize(viewPosition - vec3(worldPosition));

    // Specular factor from map (or constant material.reflectivity)
    float specMapFactor = useSpecularMap
        ? texture(specularMapSampler, pass_textureCoords).r
        : material.reflectivity;

    vec3 runningResult = vec3(0.0f);

    for (int i = 0; i < 4; i++) {
        if (light[i].constant != -1) {
            runningResult = runningResult + CalcPointLight(light[i], toLightVector[i], unitNormal, unitVectorToCamera, textureColor);
        } else {
            runningResult = runningResult + CalcDirLight(light[i], unitNormal, unitVectorToCamera, textureColor);
        }
    }

    vec4 results = vec4(runningResult, 1.0f);
    out_color = mix(vec4(skyColor, 1.0), results, visibility);

}

vec3 CalcPointLight(Light light, vec3 lightVector, vec3 unitNormal, vec3 unitVectorToCamera, vec4 totalColor) {
    vec3 unitLightVector = normalize(unitVectorToCamera);

    float nDot1 = dot(unitNormal, unitLightVector);
    float brightness = max(nDot1, 0.0);

    vec3 lightDirection = -unitLightVector;
    vec3 reflectedLightDirection = reflect(lightDirection, unitNormal);
    float specularFactor = dot(unitVectorToCamera, reflectedLightDirection);

    specularFactor = max(specularFactor, 0.0);
    float dampedFactor = pow(specularFactor, material.shininess);

    // attenuation (looks okay)
    float distance = length(lightVector);// checks out
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    //        dampedFactor = max(dampedFactor, .65);

    vec3 ambient = (light.ambient * totalColor.rgb);
    vec3 diffuse = (brightness * light.diffuse);
    vec3 specular = (dampedFactor * material.reflectivity * light.specular);

    ambient *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;

    return ambient + diffuse + specular;
}

vec3 CalcDirLight(Light light, vec3 unitNormal, vec3 unitVectorToCamera, vec4 totalColor)
{
    if (dot(unitNormal, unitVectorToCamera) < 0.0) {
        unitNormal = -unitNormal;
    }

    vec3 lightDirection = normalize(-unitVectorToCamera);

    // diffuse shading
    float diff = max(dot(unitNormal, lightDirection), 0.0);
    // specular shading

    vec3 reflectDirection = reflect(-lightDirection, unitNormal);
    float spec = pow(max(dot(unitVectorToCamera, reflectDirection), 0.0), material.shininess);

    // combine results
    vec3 diffuse  = light.diffuse  * diff * totalColor.rgb;
    vec3 specular = light.specular * spec * totalColor.rgb;
    vec3 ambient  = light.ambient  * totalColor.rgb;
    return (ambient + diffuse + specular);
}
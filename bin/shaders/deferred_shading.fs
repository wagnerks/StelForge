#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;

struct DirectionalLight {
    mat4 PV; //proj * view from light perspective matrix
    sampler2D shadowsMap;
    vec3 Position;
    vec2 textureSize;
};

const int MAX_SHADOWS_SIZE = 3;
uniform DirectionalLight DirLights[MAX_SHADOWS_SIZE];
uniform int shadowsCount;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
    float Radius;
};

const int NR_LIGHTS = 1000;
uniform Light lights[NR_LIGHTS];
uniform int lightsCount = 0;
uniform vec3 viewPos;

struct CascadedShadow {
    sampler2DArray shadowMap;
    vec3 direction;
    vec3 color;
    vec2 texelSize;
};

uniform CascadedShadow cascadedShadow;
uniform int cascadeCount;   // number of frusta - 1

uniform float farPlane;

uniform mat4 view;

layout (std140, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
};

uniform float cascadePlaneDistances[16];
uniform float bias = 0.001;

const int pcfCount = 2;
const float shadowForce = 0.01f;
const float layerSkipShadowForce = 0.5f;
const float defaultHideShadow = 0.5;


float ShadowCalculation(DirectionalLight light, vec3 projCoords, vec3 FragPos, vec3 lightDir, vec3 normal) {
    float vertexNormalToLight = dot(normal, lightDir);
    float normalKoef = 1 - (vertexNormalToLight + 1.0) * 0.5;
    if (normalKoef <= 0.0){
        return 1.0;
    }

    if (projCoords.z < 0.0){
        return 0.0;
    }
    float shadow = 0.f; 
    
    // PCF
    for(int y = -pcfCount; y <= pcfCount; ++y) { //pcf only for y axis
        float pcfDepth = texture(light.shadowsMap, projCoords.xy + vec2(0.f, y) * cascadedShadow.texelSize).r; 
        shadow += projCoords.z - bias > pcfDepth  ? 1.0 : 0.0;        
    }    

    return normalKoef * shadow * shadowForce;
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float ShadowCascadedCalculation(vec3 fragPosWorldSpace, vec3 Normal, float curShadow) {
    float vertexNormalToLight = dot(Normal, cascadedShadow.direction);
    float koef = 1 - (vertexNormalToLight + 1.0) * 0.5; //1 when parallel, 0.5 if normal, 0 if divergent
    if (koef <= 0.0){
        return 1.0;
    }
    
    // select cascade layer
    int layer = 0;
    vec4 fragPosLightSpace;
    vec3 projCoords;
    for (; layer < cascadeCount; ++layer) {
        fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; // perform perspective divide
        projCoords = projCoords * 0.5 + 0.5; // transform to [0,1] range

        if (projCoords.x > 0.01 && projCoords.x < 0.99 && projCoords.y > 0.01 && projCoords.y < 0.99 && projCoords.z > 0.01 && projCoords.z < 0.99){
            break;
        }
    }
    // get depth of current fragment from light's perspective
    if (projCoords.z < 0.0) { //projCoords.z == currentDepth
        return 0.0; // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    }
    
    float shadow = 0.0; 
    
    for(int y = -pcfCount; y <= pcfCount; ++y) {
        for(int x = -pcfCount; x <= pcfCount; ++x) {
            float pcfDepth = texture(cascadedShadow.shadowMap, vec3(projCoords.xy + vec2(x, y) * cascadedShadow.texelSize, layer)).r;
            shadow += (projCoords.z - bias) > pcfDepth ? 0.01 : 0.0;    
        }
    }  

    return min(koef, shadow);
}

vec3 calculateLightDiffuse(vec3 Normal, vec3 lightDirection, vec3 Diffuse, vec3 lightColor){
    return max(dot(Normal, lightDirection), 0.0) * Diffuse * lightColor;
}

vec3 calculateSpecular(vec3 Normal, vec3 lightDirection, vec3 viewDirection, vec3 lightColor, float Specular){
    return pow(max(dot(Normal, normalize(lightDirection + viewDirection)), 0.0), 64.0) * lightColor * Specular;
}

vec3 calculateLighting(vec3 Normal, vec3 lightDirection, vec3 viewDirection, vec3 Diffuse, vec3 lightColor, float Specular){
    return calculateLightDiffuse(Normal, -lightDirection, Diffuse, lightColor) + calculateSpecular(Normal, -lightDirection, viewDirection, lightColor, Specular);
}

void main() {
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;
    float AmbientOcclusion = texture(ssao, TexCoords).r;

    float shadow = 0.f;
    vec3 lighting = vec3(Diffuse * 0.3f * AmbientOcclusion);
    
    vec3 viewDir  = normalize(viewPos - FragPos);

    shadow += ShadowCascadedCalculation(FragPos, Normal, 0.f);
    if (shadow == 1.0){
        discard;
    }

    lighting += calculateLighting(Normal, cascadedShadow.direction, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.7f; 
 
    for (int i = 0; i < shadowsCount; i++) {
        vec4 shadowCoord = DirLights[i].PV * vec4(FragPos,1.0);
        vec3 projCoords = shadowCoord.xyz / shadowCoord.w * 0.5 + 0.5;        
        vec3 lightDir = normalize(FragPos - DirLights[i].Position);
        lighting += calculateLighting(Normal, lightDir, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.1; 
        if (lighting.x == 0.f && lighting.y == 0.f && lighting.z == 0.f){
            continue; //if here is fully shadowed object, why need i calculate shadow?
        }
        shadow += ShadowCalculation(DirLights[i], projCoords, FragPos, lightDir, Normal);

    }

    int lightsCount = lightsCount > NR_LIGHTS ? NR_LIGHTS : lightsCount;
    if (lightsCount > 0){
        for(int i = 0; i < lightsCount; ++i) {
            // calculate distance between light source and current fragment
            float distance = length(lights[i].Position - FragPos);
            if(distance < lights[i].Radius) {
                vec3 lightDir = normalize(lights[i].Position - FragPos);

                float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
                
                lighting += (calculateLightDiffuse(Normal, lightDir, Diffuse, lights[i].Color) + calculateSpecular(Normal, lightDir, viewDir, lights[i].Color, Specular)) * attenuation;
            }
        }
    }
    
    
    lighting *= (1-shadow);
    
    
    
    gl_FragColor = vec4(lighting , 1.0);
}
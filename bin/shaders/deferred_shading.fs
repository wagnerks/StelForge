#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;

struct DirectionalLight {
    mat4 PV; //proj * view from light perspective matrix
    sampler2DShadow shadowMap;
    vec3 Position;
    vec2 texelSize;
    float bias;
    int samples;
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

const int NR_LIGHTS = 50;
uniform Light lights[NR_LIGHTS];
uniform int lightsCount = 0;
uniform vec3 viewPos;

struct CascadedShadow {
    sampler2DArrayShadow shadowMap;
    vec3 direction;
    vec3 color;
    float intensity;
    vec2 texelSize[16];
    float bias[16];
    int samples[16];
};

uniform CascadedShadow cascadedShadow;
uniform int cascadeCount;   // number of frusta - 1


layout (std140, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
};


uniform float ambientColor = 1.0;



vec2 vogel_disk_sample(uint sample_index, int sample_count, float angle)
{
  const float golden_angle = 2.399963f; // radians
  float r                  = sqrt(sample_index + 0.5f) / sqrt(sample_count);
  float theta              = sample_index * golden_angle + angle;
  float sine, cosine;

  sine = sin(theta);
  cosine = cos(theta);
  return vec2(cosine, sine) * r;
}

float TechniqueVogelCascaded(int layer, float bias, vec3 projCoords) {
    float shadow = 0.f; 

    float penumbra        = 1.0f;
    
    float temporal_offset = 0.0;
    float temporal_angle  = temporal_offset * 3.14 * 2;
    float g_shadow_filter_size = 1.0;
    float g_shadow_samples_rpc = 1.0 / cascadedShadow.samples[layer];

    

    for (uint i = 0; i < cascadedShadow.samples[layer]; i++) {
        vec2 offset = vogel_disk_sample(i, cascadedShadow.samples[layer], temporal_angle) * (cascadedShadow.texelSize[layer]) *  g_shadow_filter_size * penumbra;
        
        float depth = texture(cascadedShadow.shadowMap, vec4(projCoords.xy + offset, layer, projCoords.z));
        shadow += projCoords.z + bias > depth ? 1.0 : 0.0;   
    } 

    return shadow * g_shadow_samples_rpc;
}

float TechniqueVogel(DirectionalLight light, float bias, vec3 projCoords) {
    float shadow = 0.f; 

    float penumbra        = 1.0f;
    
    float temporal_offset = 0.0;
    float temporal_angle  = temporal_offset * 3.14 * 2;
    float g_shadow_filter_size = 1.0;
    float g_shadow_samples_rpc = 1.0 / light.samples;

    

    for (uint i = 0; i < light.samples; i++) {
        vec2 offset = vogel_disk_sample(i, light.samples, temporal_angle) * (light.texelSize) *  g_shadow_filter_size * penumbra;
        
        float depth = texture(light.shadowMap, vec3(projCoords.xy + offset, projCoords.z));
        shadow += projCoords.z + bias > depth ? 1.0 : 0.0;   
    } 

    return shadow * g_shadow_samples_rpc;
}

float ShadowCascadedCalculation(vec3 fragPosWorldSpace, vec3 Normal) {
    float vertexNormalToLight = dot(Normal, cascadedShadow.direction);
    float koef = 1 - (vertexNormalToLight + 1.0) * 0.5; //1 when parallel, 0.5 if normal, 0 if divergent
    
    if (koef <= 0.5){
        return 1.0;
        return 1 - koef * 2.0;
    }

    // select cascade layer

    vec4 fragPosLightSpace;
    vec3 projCoords;
    float minX = 0.0;
    float maxX = 1.0;
    int layer = 0;
    for (; layer < cascadeCount + 1; ++layer) {
        fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; // perform perspective divide
        projCoords = projCoords * 0.5 + 0.5; // transform to [0,1] range
        projCoords.z -= cascadedShadow.bias[layer] * cascadedShadow.texelSize[layer].x;
        if (projCoords.x > minX && projCoords.x < maxX && projCoords.y > minX && projCoords.y < maxX && projCoords.z > minX && projCoords.z < maxX) {
            break;
        }
    }
   
    if (layer  > cascadeCount){
        return koef;
    }
    
    if (projCoords.z > 1.0){
        return koef;
    }

    float bias = cascadedShadow.bias[layer] * tan(acos(dot(Normal, cascadedShadow.direction)));

    float kek = koef * 2.0 - 1;

    return max(1 - kek, TechniqueVogelCascaded(layer, bias, projCoords));
}


float ShadowCalculation(DirectionalLight light, vec3 projCoords, vec3 lightDir, vec3 normal) {
    float vertexNormalToLight = dot(normal, lightDir);
    float koef = 1 - (vertexNormalToLight + 1.0) * 0.5; //1 when parallel, 0.5 if normal, 0 if divergent
    
    if (koef <= 0.5){
        return 1 - koef * 2.0;
    }

    if (projCoords.z > 1.0){
        return koef;
    }

    float bias = light.bias * tan(acos(dot(normal, lightDir)));;
    return TechniqueVogel(light, bias, projCoords);
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
    vec3 lighting = vec3(Diffuse * ambientColor);
    
    vec3 viewDir  = normalize(viewPos - FragPos);

    shadow += ShadowCascadedCalculation(FragPos, Normal);


    lighting += calculateLighting(Normal, cascadedShadow.direction, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.5f; 
 
    for (int i = 0; i < shadowsCount; i++) {
        vec4 shadowCoord = DirLights[i].PV * vec4(FragPos,1.0);
        vec3 projCoords = shadowCoord.xyz / shadowCoord.w * 0.5 + 0.5;        
        vec3 lightDir = normalize(FragPos - DirLights[i].Position);
        lighting += calculateLighting(Normal, lightDir, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.1; 
        if (lighting.x == 0.f && lighting.y == 0.f && lighting.z == 0.f){
            continue; //if here is fully shadowed object, why need i calculate shadow?
        }
        shadow += ShadowCalculation(DirLights[i], projCoords, lightDir, Normal);

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

    lighting *= (1 - (0.9 * shadow));
    const float gamma = 1.0;
    float exposure = 1.2;
  
    // exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-lighting * exposure);
    // gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    
    
    
    

    FragColor = vec4(mapped , 1.0);
}
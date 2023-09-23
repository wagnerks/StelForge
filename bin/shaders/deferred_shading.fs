#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;
uniform sampler2D gOutlines;

struct PointLight {
    sampler2DArrayShadow shadowMap;
    vec3 Position;
    vec2 texelSize;
    float bias;
    int samples;
    float radius;
    vec3 direction[6];
};

uniform PointLight pointLight;

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
uniform Light lights[ NR_LIGHTS ];
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

layout (std140, binding = 3) uniform PointLightMatrices
{
    mat4 pointLightMatrices[16];
};

uniform float ambientColor = 1.0;
uniform float shadowIntensity = 1.0;



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

    const float penumbra        = 1.0f;
    
    const float temporal_offset = 0.0;
    const float temporal_angle  = temporal_offset * 3.14 * 2;
    const float g_shadow_filter_size = 1.0;
    const float g_shadow_samples_rpc = 1.0 / cascadedShadow.samples[layer];

    

    for (uint i = 0; i < cascadedShadow.samples[layer]; i++) {
        const vec2 offset = vogel_disk_sample(i, cascadedShadow.samples[layer], temporal_angle) * (cascadedShadow.texelSize[layer]) *  g_shadow_filter_size * penumbra;
        
        const float depth = texture(cascadedShadow.shadowMap, vec4(projCoords.xy + offset, layer, projCoords.z));
        shadow += projCoords.z + bias > depth ? 1.0 : 0.0;   
    } 

    return shadow * g_shadow_samples_rpc;
}

float TechniqueVogelPoint(int layer, float bias, vec3 projCoords) {
    float shadow = 0.f; 

    const float penumbra        = 1.0f;
    
    const float temporal_offset = 0.0;
    const float temporal_angle  = temporal_offset * 3.14 * 2;
    const float g_shadow_filter_size = 1.0;
    const float g_shadow_samples_rpc = 1.0 / pointLight.samples;

    

    for (uint i = 0; i < pointLight.samples; i++) {
        const vec2 offset = vogel_disk_sample(i, pointLight.samples, temporal_angle) * (pointLight.texelSize) *  g_shadow_filter_size * penumbra;
        
        const float depth = texture(pointLight.shadowMap, vec4(projCoords.xy + offset, layer+6, projCoords.z));
        shadow += projCoords.z + bias > depth ? 1.0 : 0.0;   
    } 

    return shadow * g_shadow_samples_rpc;
}


float TechniqueVogel(DirectionalLight light, float bias, vec3 projCoords) {
    float shadow = 0.f; 

    const float penumbra        = 1.0f;
    
    const float temporal_offset = 0.0;
    const float temporal_angle  = temporal_offset * 3.14 * 2;
    const float g_shadow_filter_size = 1.0;
    const float g_shadow_samples_rpc = 1.0 / light.samples;

    
    return 1.0;
    for (uint i = 0; i < light.samples; i++) {
        const vec2 offset = vogel_disk_sample(i, light.samples, temporal_angle) * (light.texelSize) *  g_shadow_filter_size * penumbra;
        
        const float depth = texture(light.shadowMap, vec3(projCoords.xy + offset, projCoords.z));
        shadow += projCoords.z + 0.0001 > depth ? 1.0 : 0.0;   
    } 

    return shadow * g_shadow_samples_rpc;
}

float ShadowCascadedCalculation(vec3 fragPosWorldSpace, vec3 Normal) { //return 0.0 - light, 1.0 - dark
    // select cascade layer
    vec4 fragPosLightSpace;
    vec3 projCoords;

    int layer = 0;
    for (; layer < cascadeCount + 1; ++layer) {
        fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; // perform perspective divide
        projCoords = projCoords * 0.5 + 0.5; // transform to [0,1] range
        projCoords.z -= cascadedShadow.bias[layer] * cascadedShadow.texelSize[layer].x; //fix light line on corners, fix bias at the end of depth
        if (projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0 && projCoords.z > 0.0 && projCoords.z < 1.0) {
            break;
        }
    }
       
    if (layer > cascadeCount){
        return 0.0;//light, cause all what is not under sun should be light by default
    }

    const float illumination = -dot(Normal, cascadedShadow.direction); // -1 = darkest, 1 = lightest
    const float illuminationKoef = 1.0 - illumination; // 0 - dark, 1 - light
    
    float bias = cascadedShadow.bias[layer] * (-illumination);
   
    return TechniqueVogelCascaded(layer, bias, projCoords);
}

float PointLightCalculation(vec3 fragPosWorldSpace, vec3 Normal) {//return 0.0 - light, 1.0 - dark
    vec4 fragPosLightSpace;
    vec3 projCoords;

    const vec3 lightDir  = normalize(pointLight.Position - fragPosWorldSpace);
    const float illumination = -dot(Normal, -lightDir); // -1 = darkest, 1 = lightest
    float bias = 0.1f * (-illumination);

    int layer = 0;
    for (; layer < 6 + 1; ++layer) {
        fragPosLightSpace = pointLightMatrices[layer+6] * vec4(fragPosWorldSpace, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; // perform perspective divide
        projCoords = projCoords * 0.5 + 0.5; // transform to [0,1] range
        projCoords.z -= 0.0004f * pointLight.texelSize.x * illumination; //fix light line on corners
        if (projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0 && projCoords.z > 0.0 && projCoords.z < 1.0) {
            break;
        }
    }
    
    if (layer > 6){
        return 0.f;//it means that fragment further then light radius, so it is dark
    }

    return TechniqueVogelPoint(layer, bias, projCoords);
}

float ShadowCalculation(DirectionalLight light, vec3 projCoords, vec3 lightDir, vec3 normal) {
    const float vertexNormalToLight = dot(normal, lightDir);
    const float koef = 1 - (vertexNormalToLight + 1.0) * 0.5; //1 when parallel, 0.5 if normal, 0 if divergent
    
    if (koef >= 0.5){
        return 1.0;
        return 1 - koef * 2.0;
    }

    if (projCoords.z > 1.0){
        return 1 - koef * 2.0;
    }

    const float bias = light.bias * tan(acos(dot(normal, lightDir)));;
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


vec2 calculateIllumination(vec3 lightDir, vec3 normal){
    const float illumination = -dot(normal, lightDir); // -1 = darkest, 1 = lightest
    float illuminationKoef = illumination; // 0 - dark, 1 - light
    if (illuminationKoef < 0.0){
        illuminationKoef = 0.0;
    }
    return vec2(illumination, 1.0 - illuminationKoef);
}

float customMix(float x, float y, float a) {
    return x * (1.0 - a) + y * a;
}

void main() {
    // retrieve data from gbuffer
    const vec3 FragPos = texture(gPosition, TexCoords).rgb;
    const vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    const float Specular = texture(gAlbedoSpec, TexCoords).a;
    const float AmbientOcclusion = texture(ssao, TexCoords).r;
    
    float shadow = 0.f;
    vec3 lighting = vec3(Diffuse * ambientColor);
    
    const vec3 viewDir  = normalize(viewPos - FragPos);

    

    vec2 illuminationPoint = calculateIllumination(-normalize(pointLight.Position - FragPos), Normal);

    vec2 illuminationSun = calculateIllumination(cascadedShadow.direction, Normal);
    lighting += calculateLighting(Normal, cascadedShadow.direction, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.5f; 

    float illum = illuminationSun.y;
    illum = max(illum, illuminationPoint.y); // 1 means light, 0 means dark
    
    shadow = illum;

    if (illum > 0.0){
        shadow = max(illuminationSun.y, ShadowCascadedCalculation(FragPos, Normal));
        float k = length(pointLight.Position - FragPos) / pointLight.radius;
        if (k <= 1.0){
            shadow = customMix(shadow, max(illuminationPoint.y, PointLightCalculation(FragPos, Normal)), 1.0 - k);
        }
    }

    for (int i = 0; i < shadowsCount; i++) {
        const vec4 shadowCoord = DirLights[i].PV * vec4(FragPos,1.0);
        const vec3 projCoords = shadowCoord.xyz / shadowCoord.w * 0.5 + 0.5;        
        const vec3 lightDir = normalize(FragPos - DirLights[i].Position);
        lighting += calculateLighting(Normal, lightDir, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.1; 
        if (lighting.x == 0.f && lighting.y == 0.f && lighting.z == 0.f){
            continue; //if here is fully shadowed object, why need i calculate shadow?
        }
        shadow += ShadowCalculation(DirLights[i], projCoords, lightDir, Normal);

    }

    const int lightsCount = lightsCount > NR_LIGHTS ? NR_LIGHTS : lightsCount;

    for(int i = 0; i < lightsCount; ++i) {
        // calculate distance between light source and current fragment
        const float distance = length(lights[i].Position - FragPos);
        if(distance < lights[i].Radius) {
            const vec3 lightDir = normalize(lights[i].Position - FragPos);

            const float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
            
            lighting += (calculateLightDiffuse(Normal, lightDir, Diffuse, lights[i].Color) + calculateSpecular(Normal, lightDir, viewDir, lights[i].Color, Specular)) * attenuation;
        }
    }
    

    lighting *= (1.0-(shadowIntensity * shadow));
    //lighting *= AmbientOcclusion;
    const float gamma = 1.0;
    const float exposure = 1.2;
    

    const float outlines = texture(gOutlines, TexCoords).b;


    // exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-lighting * exposure);
    // gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));
    mapped.g += outlines;

    FragColor = vec4(mapped, 1.0);
}
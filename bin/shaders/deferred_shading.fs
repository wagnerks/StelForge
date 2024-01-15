#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

uniform vec3 viewPos;

uniform float ambientColor = 1.0;
uniform float shadowIntensity = 1.0;

uniform float drawDistance = 5000.0;
uniform float fogStart = 4500.0;

const int MAX_POINT_LIGHTS_SIZE = 6;
uniform int pointLightsSize = 0;
uniform int cascadeCount = 0;   // number of frusta - 1

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;
uniform sampler2D gOutlines;

uniform sampler2DArrayShadow PointLightShadowMapArray;
struct PointLight {
    vec3 Position;
    vec2 texelSize;
    float bias;
    int samples;
    float radius;
    int offset; //all point lights store in one 2D array, and one matricec array, so here should be offset for matrix and shadowMap layer

    vec3 Color;
    float Linear;
    float Quadratic;
    int Type; //0 - point, 1 - directional
    int Layers;
};
uniform PointLight pointLight[MAX_POINT_LIGHTS_SIZE];

struct CascadedShadow {
    sampler2DArrayShadow shadowMap;
    vec3 direction;
    vec3 color;
    float intensity;
    vec2 texelSize[6];
    float bias[6];
    int samples[6];
};
uniform CascadedShadow cascadedShadow;

layout (std140, binding = 0) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[6];
};

layout (std140, binding = 3) uniform PointLightMatrices {
    mat4 pointLightMatrices[36];
};



vec2 vogel_disk_sample(uint sample_index, int sample_count, float angle) {
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

float TechniqueVogelPoint(PointLight light, int layer, float bias, vec3 projCoords) {
    float shadow = 0.f; 

    const float penumbra        = 1.0f;
    
    const float temporal_offset = 0.0;
    const float temporal_angle  = temporal_offset * 3.14 * 2;
    const float g_shadow_filter_size = 1.0;
    const float g_shadow_samples_rpc = 1.0 / light.samples;

    

    for (uint i = 0; i < light.samples; i++) {
        const vec2 offset = vogel_disk_sample(i, light.samples, temporal_angle) * (light.texelSize) *  g_shadow_filter_size * penumbra;
        
        const float depth = texture(PointLightShadowMapArray, vec4(projCoords.xy + offset, layer + light.offset, projCoords.z));
        shadow += projCoords.z + bias > depth ? 1.0 : 0.0;   
    } 

    return shadow * g_shadow_samples_rpc;
}

float ShadowCascadedCalculation(vec3 fragPosWorldSpace, vec3 Normal) { //return 0.0 - light, 1.0 - dark
    // select cascade layer
    vec4 fragPosLightSpace;
    vec3 projCoords;

    int layer = 0;
    for (; layer < cascadeCount; ++layer) {
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

float PointLightCalculation(PointLight light, vec3 fragPosWorldSpace, vec3 Normal) {//return 0.0 - light, 1.0 - dark
    vec4 fragPosLightSpace;
    vec3 projCoords;

    const vec3 lightDir  = normalize(light.Position - fragPosWorldSpace);
    const float illumination = -dot(Normal, -lightDir); // -1 = darkest, 1 = lightest
    float bias = light.bias * (-illumination);

    int layer = 0;
    for (; layer < light.Layers; ++layer) {
        fragPosLightSpace = pointLightMatrices[layer + light.offset] * vec4(fragPosWorldSpace, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; // perform perspective divide
        projCoords = projCoords * 0.5 + 0.5; // transform to [0,1] range
        projCoords.z -= light.bias * light.texelSize.x * illumination; //fix light line on corners
        if (projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0 && projCoords.z > 0.0 && projCoords.z < 1.0) {
            break;
        }
    }

    if (layer >= light.Layers){
        return 0.f;//it means that fragment further then light radius, so it is dark
    }

    return TechniqueVogelPoint(light, layer, bias, projCoords);
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
    const float Depth = texture(gNormal, TexCoords).a;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    const float Specular = texture(gAlbedoSpec, TexCoords).a;
    const float AmbientOcclusion = texture(ssao, TexCoords).r;
    
    const vec3 viewDir  = normalize(viewPos - FragPos);
    vec3 lighting = vec3(Diffuse * ambientColor);
    lighting += calculateLighting(Normal, cascadedShadow.direction, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.5f; 
    

    vec2 illuminationSun = calculateIllumination(cascadedShadow.direction, Normal); 

    float illum = illuminationSun.y;
    for (int idx = 0; idx < pointLightsSize; idx++){
        vec2 illuminationPoint = calculateIllumination(-normalize(pointLight[idx].Position - FragPos), Normal);
        illum = max(illum, illuminationPoint.y); // 1 means light, 0 means dark
    }

    float shadow = 0.f;
    if (illum > 0.0){
        shadow = max(illuminationSun.y, ShadowCascadedCalculation(FragPos, Normal));

        for (int idx = 0; idx < pointLightsSize; idx++){
            float distance = length(pointLight[idx].Position - FragPos);
            float k = distance / pointLight[idx].radius;
            if (k >= 1.0){
                continue;
            }
            vec3 lightDir = -normalize(pointLight[idx].Position - FragPos);
            vec2 point = calculateIllumination(lightDir, Normal);
            //if (pointLight[idx].Type == 0){
                shadow = customMix(shadow, max(point.y, PointLightCalculation(pointLight[idx], FragPos, Normal)) * pow(1.0 - k, 0.2), 1.0 - k);
           
                const float attenuation = 1.0 / (1.0 + pointLight[idx].Linear * distance + pointLight[idx].Quadratic * distance * distance);
            
                lighting += (calculateLightDiffuse(Normal, -lightDir, Diffuse, pointLight[idx].Color) + calculateSpecular(Normal, -lightDir, viewDir, pointLight[idx].Color, Specular)) * attenuation;
                
            // } 
            // else{
            //     shadow *= PointLightCalculation(pointLight[idx], FragPos, Normal);
            // }
        }
    }

    lighting *= (1.0 - (shadowIntensity * shadow * 0.9));
    //lighting *= AmbientOcclusion;
    const float gamma = 1.0;
    const float exposure = 1.2;
    
    const float outlines = texture(gOutlines, TexCoords).b;

    // exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-lighting * exposure);
    
    vec3 bluredLight = vec3(texture(gOutlines, TexCoords).a);
    mapped.rgb = mapped.rgb+ bluredLight.rgb;
   
    
    // gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));
    mapped.g += outlines;

    //FragColor = vec4(mapped, 1.0);

    float fogFactor = smoothstep(fogStart, drawDistance, Depth);
    vec3 fogColor = vec3(0.0,0.0,0.0);
    FragColor = mix(vec4(mapped, 1.0), vec4(fogColor, 1.0), fogFactor);

}

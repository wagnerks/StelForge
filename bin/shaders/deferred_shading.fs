#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gAlbedoSpec;
layout(binding = 3) uniform sampler2D ssao;
layout(binding = 4) uniform sampler2D shadows;
layout(binding = 5) uniform sampler2D gOutlines;

layout(binding = 6) uniform sampler2DArray shadowMapDepth;
layout(binding = 7) uniform sampler2DArrayShadow PointLightShadowMapArray;
layout(binding = 8) uniform sampler2DArrayShadow shadowMap;
layout(binding = 12) uniform samplerCube irradianceMap;
layout(binding = 13) uniform samplerCube prefilterMap;
layout(binding = 14) uniform sampler2D brdfLUT;

layout (std140, binding = 15) uniform PushConstants {
    vec3 viewPos;
    float shadowIntensity;

    float fogStart;
    float drawDistance;
    int pointLightsSize;
    int cascadeCount;

    vec3 ambientColor;
    float ssaoAmount;
} pc;


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

const int MAX_POINT_LIGHTS_SIZE = 12;

layout (std140, binding = 10) uniform pointLightsBlock {
    PointLight pointLight[MAX_POINT_LIGHTS_SIZE];
} plb;

struct CascadData{
    // xy texelSize, z bias, w samples
    vec4 cascade;
};

layout (std140, binding = 9) uniform cascadedShadowBlock {
    vec4 direction_intensity; // xyz direction, w intensity
    vec4 color_and_misc; 
    CascadData data[6];
    vec4 cascadeSplits;
    vec4 cascadeSplits2;
} cascadedShadow;

layout (std140, binding = 11) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[6];
};

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

layout (std140, binding = 3) uniform PointLightMatrices {
    mat4 pointLightMatrices[72];
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

float interleavedGradientNoise(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

float findAverageBlockerDepth(int layer, vec3 proj, float searchRadius, float angle) {
    const int BLOCKER_SAMPLES = 8;
    float sum = 0.0;
    int count = 0;
    for (int i = 0; i < BLOCKER_SAMPLES; ++i) {
        vec2 offset = vogel_disk_sample(uint(i), BLOCKER_SAMPLES, angle) * searchRadius;
        float depth = texture(shadowMapDepth, vec3(proj.xy + offset, float(layer))).r;
        if (depth < proj.z) {
            sum += depth;
            ++count;
        }
    }
    if (count == 0) {
        return -1.0;
    }
    return sum / float(count);
}

float TechniqueVogelCascaded(int layer, float bias, vec3 projCoords) {
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 0.0;
    }

    float angle = interleavedGradientNoise(gl_FragCoord.xy) * 6.28318530718;
    float uvTexel = max(cascadedShadow.data[layer].cascade.x, 1.0 / 2048.0);
    projCoords.z -= max(bias, uvTexel * 1.25);

    const int sampleCount = 4;
    float radius = uvTexel * 1.5;
    float shadow = 0.0;
    for (uint i = 0; i < uint(sampleCount); ++i) {
        vec2 offset = vogel_disk_sample(i, sampleCount, angle) * radius;
        float lit = texture(shadowMap, vec4(projCoords.xy + offset, float(layer), projCoords.z));
        shadow += 1.0 - lit;
    }
    return shadow / float(sampleCount);
}

float TechniqueVogelPoint(PointLight light, int layer, float bias, vec3 projCoords) {
    float shadow = 0.f; 

    const float penumbra        = 1.0f;
    
    const float temporal_offset = 0.0;
    const float temporal_angle  = temporal_offset * 3.14 * 2;
    const float g_shadow_filter_size = 1.0;
    const int sampleCount = clamp(light.samples, 1, 64);
    const float g_shadow_samples_rpc = 1.0 / float(sampleCount);

    

    for (int i = 0; i < sampleCount; i++) {
        const vec2 offset = vogel_disk_sample(uint(i), sampleCount, temporal_angle) * (light.texelSize) *  g_shadow_filter_size * penumbra;
        
        const float lit = texture(PointLightShadowMapArray, vec4(projCoords.xy + offset, layer + light.offset, projCoords.z + bias));
        shadow += 1.0 - lit;   
    } 

    return shadow * g_shadow_samples_rpc;
}

float cascadeSplit(int idx) {
    if (idx < 4) {
        return cascadedShadow.cascadeSplits[idx];
    }
    return cascadedShadow.cascadeSplits2[idx - 4];
}

vec3 worldToShadowUv(int layer, vec3 worldPos) {
    vec4 light = lightSpaceMatrices[layer] * vec4(worldPos, 1.0);
    vec3 proj = light.xyz / light.w;
    return proj * 0.5 + 0.5;
}

float sampleCascade(int layer, vec3 worldPos, vec3 Normal) {
    vec3 proj = worldToShadowUv(layer, worldPos);
    float illumination = -dot(Normal, cascadedShadow.direction_intensity.xyz);
    float nDotL = clamp(illumination, 0.0, 1.0);
    float bias = cascadedShadow.data[layer].cascade.z * mix(2.0, 0.35, nDotL);
    return TechniqueVogelCascaded(layer, bias, proj);
}

bool shadowUvOk(vec3 proj) {
    return proj.x >= 0.0 && proj.x <= 1.0 && proj.y >= 0.0 && proj.y <= 1.0 && proj.z >= 0.0 && proj.z <= 1.0;
}

int cascadeForViewZ(float viewZ, int cc) {
    int layer = cc - 1;
    for (int i = 0; i < cc; ++i) {
        if (viewZ < cascadeSplit(i)) {
            return i;
        }
    }
    return layer;
}

int cascadeContaining(vec3 worldPos, int cc, int preferred) {
    if (preferred >= 0 && preferred < cc && shadowUvOk(worldToShadowUv(preferred, worldPos))) {
        return preferred;
    }
    for (int i = 0; i < cc; ++i) {
        if (shadowUvOk(worldToShadowUv(i, worldPos))) {
            return i;
        }
    }
    return -1;
}

float ShadowCascadedCalculation(vec3 fragPosWorldSpace, vec3 Normal)
{
    int cc = pc.cascadeCount;
    if (cc < 1) {
        cc = int(cascadedShadow.color_and_misc.w + 0.5);
    }
    if (cc < 1) {
        cc = 4;
    }

    float viewZ = abs((matrices.view * vec4(fragPosWorldSpace, 1.0)).z);
    int layer = cascadeContaining(fragPosWorldSpace, cc, cascadeForViewZ(viewZ, cc));
    if (layer < 0) {
        return 0.0;
    }

    float shadow = sampleCascade(layer, fragPosWorldSpace, Normal);
    if (layer + 1 >= cc) {
        return shadow;
    }

    float split = cascadeSplit(layer);
    float prev = layer > 0 ? cascadeSplit(layer - 1) : 0.0;
    float band = max((split - prev) * 0.18, 0.35);
    float blend = clamp((viewZ - (split - band)) / band, 0.0, 1.0);
    if (blend <= 0.0 || !shadowUvOk(worldToShadowUv(layer + 1, fragPosWorldSpace))) {
        return shadow;
    }
    return mix(shadow, sampleCascade(layer + 1, fragPosWorldSpace, Normal), blend);
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

const float PI = 3.14159265;
const float MAX_REFLECTION_LOD = 4.0;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 cookTorrance(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 F0, float roughness, float metallic, vec3 radiance) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 specular = (NDF * G * F) / max(4.0 * max(dot(N, V), 0.0) * NdotL, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec2 calculateIllumination(vec3 lightDir, vec3 normal){
    const float illumination = -dot(normal, lightDir);
    float illuminationKoef = illumination;
    if (illuminationKoef < 0.0){
        illuminationKoef = 0.0;
    }
    return vec2(illumination, 1.0 - illuminationKoef);
}

float customMix(float x, float y, float a) {
    return x * (1.0 - a) + y * a;
}

void main() {
    vec4 posSample = texture(gPosition, TexCoords);
    const vec3 FragPos = posSample.rgb;
    const vec3 Normal = normalize(texture(gNormal, TexCoords).rgb);
    const float Depth = texture(gNormal, TexCoords).a;
    vec4 albedoSample = texture(gAlbedoSpec, TexCoords);
    vec3 albedo = albedoSample.rgb;
    float roughness = clamp(albedoSample.a, 0.04, 1.0);
    float metallic = clamp(posSample.a, 0.0, 1.0);
    const float AmbientOcclusion = texture(ssao, TexCoords).r;

    const vec3 V = normalize(pc.viewPos - FragPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lsun = normalize(-cascadedShadow.direction_intensity.xyz);
    vec3 Lo = cookTorrance(
        Normal, V, Lsun,
        albedo, F0, roughness, metallic,
        cascadedShadow.color_and_misc.xyz * PI);

    float nDotLSun = max(dot(Normal, Lsun), 0.0);
    float shadow = 0.0;
    if (nDotLSun > 0.001) {
        shadow = ShadowCascadedCalculation(FragPos, Normal);
    }

    for (int idx = 0; idx < pc.pointLightsSize; idx++) {
        float distance = length(plb.pointLight[idx].Position - FragPos);
        float k = distance / plb.pointLight[idx].radius;
        if (k >= 1.0) {
            continue;
        }
        vec3 Lp = normalize(plb.pointLight[idx].Position - FragPos);
        float nDotLp = max(dot(Normal, Lp), 0.0);
        if (nDotLp > 0.001) {
            float pointShadow = PointLightCalculation(plb.pointLight[idx], FragPos, Normal) * pow(1.0 - k, 0.2);
            shadow = customMix(shadow, pointShadow, 1.0 - k);
        }
        const float attenuation = 1.0 / (1.0 + plb.pointLight[idx].Linear * distance + plb.pointLight[idx].Quadratic * distance * distance);
        Lo += cookTorrance(Normal, V, Lp, albedo, F0, roughness, metallic, plb.pointLight[idx].Color * attenuation);
    }

    const float shadowFactor = 1.0 - pc.shadowIntensity * shadow;
    Lo *= shadowFactor;

    vec3 F = fresnelSchlickRoughness(max(dot(Normal, V), 0.0), F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 irradiance = texture(irradianceMap, Normal).rgb;
    vec3 diffuse = irradiance * albedo;
    vec3 R = reflect(-V, Normal);
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(Normal, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    float ao = mix(1.0, AmbientOcclusion, pc.ssaoAmount);
    vec3 ambient = kD * diffuse * ao + specular;
    ambient *= mix(0.7, 1.0, shadowFactor);
    // Follow sun brightness so a dim sun doesn't leave bright IBL wash.
    float sunAmt = clamp(dot(pc.ambientColor, vec3(0.333)), 0.0, 1.0);
    ambient *= mix(0.05, 1.0, sunAmt);

    vec3 lighting = ambient + Lo;

    const float outlines = texture(gOutlines, TexCoords).b;
    vec3 fogColor = pc.ambientColor * 0.25 + vec3(0.10, 0.14, 0.20);
    float heightFog = 1.0 - exp(-max(0.0, 6.0 - FragPos.y) * 0.05);
    float distFog = smoothstep(pc.fogStart, pc.drawDistance, Depth);
    float fogFactor = clamp(max(distFog, heightFog * 0.4), 0.0, 0.82);
    vec3 color = mix(lighting, fogColor, fogFactor);
    color += vec3(0.0, 0.78, 0.86) * outlines * 0.8;
    FragColor = vec4(color, 1.0);
}

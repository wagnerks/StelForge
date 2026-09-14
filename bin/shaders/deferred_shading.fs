#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D gPosition;
layout(SFE_SET(0, 1)) uniform sampler2D gNormal;
layout(SFE_SET(0, 2)) uniform sampler2D gAlbedoSpec;
layout(SFE_SET(0, 3)) uniform sampler2D ssao;
layout(SFE_SET(0, 5)) uniform sampler2D gOutlines;

layout(SFE_SET(0, 6)) uniform sampler2DArray shadowMapDepth;
layout(SFE_SET(0, 7)) uniform sampler2DArrayShadow PointLightShadowMapArray;
layout(SFE_SET(0, 8)) uniform sampler2DArrayShadow shadowMap;
layout(SFE_SET(0, 12)) uniform samplerCube irradianceMap;
layout(SFE_SET(0, 13)) uniform samplerCube prefilterMap;
layout(SFE_SET(0, 14)) uniform sampler2D brdfLUT;

layout(SFE_SET(1, 15), std140) uniform PushConstants {
    vec3 viewPos;
    float shadowIntensity;

    float fogStart;
    float drawDistance;
    int pointLightsSize;
    int cascadeCount;

    vec3 ambientColor;
    float ssaoAmount;

    vec3 viewDir;
    float iblStrength;
    float shadowAmbient;
    float padLook0;
    float padLook1;
    float debugTerm;
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

layout(SFE_SET(1, 10), std140) uniform pointLightsBlock {
    PointLight pointLight[MAX_POINT_LIGHTS_SIZE];
} plb;

struct CascadData{
    // x 1/resolution, y world size of one texel, z depth bias, w samples
    vec4 cascade;
    // x light-space depth extent of this cascade, the rest spare
    vec4 cascadeExtra;
};

layout(SFE_SET(1, 9), std140) uniform cascadedShadowBlock {
    vec4 direction_intensity; // xyz direction, w intensity
    vec4 color_and_misc; 
    CascadData data[6];
    vec4 cascadeSplits;
    vec4 cascadeSplits2;
} cascadedShadow;

layout(SFE_SET(1, 11), std140) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[6];
};

layout(SFE_SET(1, 5), std140) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

layout(SFE_SET(1, 3), std140) uniform PointLightMatrices {
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

// Jimenez's interleaved gradient noise, fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))),
// in fixed point. The float form came out different on OpenGL and Vulkan for the same pixel -- the two shader
// compilers evaluate it differently even marked precise -- so the Vogel samples it rotates, and every
// penumbra, differed between backends. Integer arithmetic has one result: the inner fract keeps 16 bits,
// and the outer multiply wraps modulo 2^32, which is exactly the outer fract.
float interleavedGradientNoise(vec2 p) {
    uvec2 pixel = uvec2(p);
    uint inner = (pixel.x * 4398u + pixel.y * 383u) & 0xFFFFu; // fract(dot(p, k)) * 2^16
    uint outer = inner * 3472289u;                             // fract(52.9829189 * inner / 2^16) * 2^32
    return float(outer >> 8u) / 16777216.0;
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
    float uvTexel = max(cascadedShadow.data[layer].cascade.x, 1.0 / 4096.0);
    projCoords.z -= max(bias, uvTexel * 1.25);

    const int sampleCount = clamp(int(cascadedShadow.data[layer].cascade.w + 0.5), 4, 16);

    // Contact hardening. A fixed filter radius gives every shadow the same soft edge, at the foot
    // of a wall and a hundred metres down its length alike, which is what makes a shadow read as
    // painted on. The penumbra of a real shadow grows with the gap between caster and receiver, so
    // measure that gap first: findAverageBlockerDepth (written long ago and never called) returns
    // the mean depth of whatever stands between this point and the sun.
    float worldTexel = max(cascadedShadow.data[layer].cascade.y, 1e-6);
    float depthRange = max(cascadedShadow.data[layer].cascadeExtra.x, 1e-4);
    float radius = uvTexel * 1.5;
    float blocker = findAverageBlockerDepth(layer, projCoords, uvTexel * 8.0, angle);
    if (blocker > 0.0) {
        // Depth difference to world units, then a world penumbra from the sun's angular size, then
        // back into UV. SUN_TAN is a touch wider than the real sun so the softening is visible.
        const float SUN_TAN = 0.02;
        float gapWorld = max(projCoords.z - blocker, 0.0) * depthRange;
        float penumbraUV = gapWorld * SUN_TAN * uvTexel / worldTexel;
        radius = clamp(penumbraUV, uvTexel * 1.2, uvTexel * 10.0);
    }
    float shadow = 0.0;
    int used = 0;
    for (uint i = 0; i < uint(sampleCount); ++i) {
        vec2 offset = vogel_disk_sample(i, sampleCount, angle) * radius;
        vec2 uv = projCoords.xy + offset;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            continue;
        }
        float lit = texture(shadowMap, vec4(uv, float(layer), projCoords.z));
        shadow += 1.0 - lit;
        used += 1;
    }
    if (used == 0) {
        return 0.0;
    }
    return shadow / float(used);
}

float TechniqueVogelPoint(PointLight light, int layer, vec3 projCoords) {
    float shadow = 0.0;

    const int sampleCount = clamp(light.samples, 1, 64);
    const float invSamples = 1.0 / float(sampleCount);
    const float angle = interleavedGradientNoise(gl_FragCoord.xy) * 6.28318530718;

    for (int i = 0; i < sampleCount; i++) {
        const vec2 offset = vogel_disk_sample(uint(i), sampleCount, angle) * light.texelSize;
        const float lit = texture(PointLightShadowMapArray, vec4(projCoords.xy + offset, layer + light.offset, projCoords.z));
        shadow += 1.0 - lit;
    }

    return shadow * invSamples;
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
    proj.xy = proj.xy * 0.5 + 0.5;
    proj.z = proj.z * 0.5 + 0.5;
    return proj;
}

float sampleCascade(int layer, vec3 worldPos, vec3 Normal) {
    float illumination = -dot(Normal, cascadedShadow.direction_intensity.xyz);
    float nDotL = clamp(illumination, 0.0, 1.0);

    // Normal-offset bias: step the lookup off the surface instead of pushing its depth away from
    // the light. A depth bias slides the shadow along the light ray, which is what detaches it
    // from the caster (peter panning); moving along the normal cannot, because the shift is
    // perpendicular to the ray. The offset grows on grazing surfaces, where one texel covers the
    // most depth. Depth bias stays only as the small residual the offset cannot cover.
    float worldTexel = cascadedShadow.data[layer].cascade.y;
    float slope = 1.0 - nDotL;
    vec3 proj = worldToShadowUv(layer, worldPos + Normal * (worldTexel * (1.0 + 3.0 * slope)));

    float bias = cascadedShadow.data[layer].cascade.z * mix(0.6, 0.15, nDotL);
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

float frustumViewZ(vec3 worldPos) {
    return abs(dot(worldPos - pc.viewPos, pc.viewDir));
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

    float viewZ = frustumViewZ(fragPosWorldSpace);
    int layer = clamp(cascadeForViewZ(viewZ, cc), 0, cc - 1);
    while (layer + 1 < cc && !shadowUvOk(worldToShadowUv(layer, fragPosWorldSpace))) {
        layer += 1;
    }
    float shadow = sampleCascade(layer, fragPosWorldSpace, Normal);
    if (layer + 1 >= cc) {
        return shadow;
    }

    float split = cascadeSplit(layer);
    float prev = layer > 0 ? cascadeSplit(layer - 1) : 0.0;
    float band = max((split - prev) * 0.14, 0.25);
    float blend = clamp((viewZ - (split - band)) / band, 0.0, 1.0);
    if (blend <= 0.0) {
        return shadow;
    }
    // Only cross-fade into the next cascade when the fragment actually lands inside it.
    // TechniqueVogelCascaded answers "lit" for out-of-range coordinates, so blending towards a
    // cascade that does not cover this fragment faded real shadows out at every boundary.
    if (!shadowUvOk(worldToShadowUv(layer + 1, fragPosWorldSpace))) {
        return shadow;
    }
    return mix(shadow, sampleCascade(layer + 1, fragPosWorldSpace, Normal), blend);
}



float PointLightCalculation(PointLight light, vec3 fragPosWorldSpace, vec3 Normal) {//return 0.0 - light, 1.0 - dark
    const vec3 lightDir = normalize(light.Position - fragPosWorldSpace);
    const float nDotL = clamp(dot(Normal, lightDir), 0.0, 1.0);
    // texelSize is the PCF kernel, not 1/mapSize. Cap the old 0.0035 NDC default —
    // that plus a second z+bias sample is what detached contact shadows (peter panning).
    const float mapTexel = 1.0 / 1024.0;
    const float ndcBias = min(max(light.bias, 0.0), mapTexel * 1.5);
    const float zBias = max(ndcBias, mapTexel * 0.6) * mix(2.0, 0.35, nDotL);

    int layer = 0;
    vec3 projCoords = vec3(0.0);
    for (; layer < light.Layers; ++layer) {
        vec4 fragPosLightSpace = pointLightMatrices[layer + light.offset] * vec4(fragPosWorldSpace, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if (projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0 &&
            projCoords.z > 0.0 && projCoords.z < 1.0) {
            break;
        }
    }

    if (layer >= light.Layers) {
        return 0.0;
    }

    projCoords.z -= zBias;
    return TechniqueVogelPoint(light, layer, projCoords);
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
    const vec3 nSample = texture(gNormal, TexCoords).rgb;
    const float nLen = length(nSample);
    const vec3 Normal = nLen > 1e-5 ? nSample / nLen : vec3(0.0, 1.0, 0.0);
    const float sssAmt = clamp((1.0 - nLen) * 2.0, 0.0, 1.0);
    const float Depth = texture(gNormal, TexCoords).a;
    vec4 albedoSample = texture(gAlbedoSpec, TexCoords);
    vec3 albedo = albedoSample.rgb;
    float roughness = clamp(albedoSample.a, 0.04, 1.0);
    vec4 pbr = sfeGBufferPbr(posSample.a);
    float metallic = pbr.x;
    float specFactor = pbr.y;
    float clearcoat = pbr.z;
    float ccRough = max(pbr.w, 0.04);
    const float AmbientOcclusion = texture(ssao, TexCoords).r;

    const vec3 V = normalize(pc.viewPos - FragPos);
    float luma = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
    float darkDiel = (1.0 - metallic) * (1.0 - smoothstep(0.03, 0.22, luma));
    darkDiel *= 1.0 - smoothstep(0.05, 0.25, clearcoat);
    roughness = mix(roughness, max(roughness, 0.62), darkDiel);
    vec3 F0 = mix(vec3(0.04 * specFactor), albedo, metallic);
    F0 *= mix(1.0, 0.35, darkDiel);

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
    vec3 irradiance = texture(irradianceMap, Normal).rgb * pc.iblStrength;
    vec3 diffuse = irradiance * albedo;
    vec3 R = reflect(-V, Normal);
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb * pc.iblStrength;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(Normal, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    specular *= mix(1.0, 0.12, darkDiel);
    specular *= 1.0 - clearcoat;
    if (clearcoat > 0.01) {
        vec3 Fc0 = vec3(0.04);
        vec3 sunCol = cascadedShadow.color_and_misc.xyz * PI;
        vec3 Fc = fresnelSchlickRoughness(max(dot(Normal, V), 0.0), Fc0, ccRough);
        float coatRough = max(ccRough, 0.08);
        Lo += cookTorrance(Normal, V, Lsun, vec3(0.0), Fc0, coatRough, 0.0, sunCol) * clearcoat;
        vec3 preCc = textureLod(prefilterMap, R, coatRough * MAX_REFLECTION_LOD).rgb * pc.iblStrength;
        vec2 brdfCc = texture(brdfLUT, vec2(max(dot(Normal, V), 0.0), coatRough)).rg;
        specular += preCc * (Fc * brdfCc.x + brdfCc.y) * clearcoat;
        specular += Fc * clearcoat * 0.28;
    }
    float ao = mix(1.0, AmbientOcclusion, pc.ssaoAmount);
    ao = mix(ao, 1.0, clearcoat);
    vec3 ambient = kD * diffuse * ao + specular;
    ambient *= mix(pc.shadowAmbient, 1.0, shadowFactor);

    // Backend-parity debugging: isolate one term so it can be captured and diffed on its own.
    // 1 shadow factor, 2 ambient/IBL, 3 direct light, 4 albedo, 5 normal, 6 AO, 7 irradiance,
    // 8 prefiltered reflection, 9 brdf LUT.
    if (pc.debugTerm > 0.5) {
        int term = int(pc.debugTerm + 0.5);
        vec3 dbg = vec3(0.0);
        if      (term == 1) dbg = vec3(shadowFactor);
        else if (term == 2) dbg = ambient;
        else if (term == 3) dbg = Lo;
        else if (term == 4) dbg = albedo;
        else if (term == 5) dbg = Normal * 0.5 + 0.5;
        else if (term == 6) dbg = vec3(ao);
        else if (term == 7) dbg = irradiance;
        else if (term == 8) dbg = prefilteredColor;
        else if (term == 9) dbg = vec3(brdf, 0.0);
        else if (term == 14) {
            // R = hardware comparison sampler result, G = manual compare on the same texel,
            // B = stored depth. If R and G disagree the comparison sampler is the problem.
            int cc = pc.cascadeCount > 0 ? pc.cascadeCount : int(cascadedShadow.color_and_misc.w + 0.5);
            int L = clamp(cascadeForViewZ(frustumViewZ(FragPos), cc), 0, cc - 1);
            while (L + 1 < cc && !shadowUvOk(worldToShadowUv(L, FragPos))) L += 1;
            vec3 sp = worldToShadowUv(L, FragPos);
            float hw = texture(shadowMap, vec4(sp.xy, float(L), sp.z));
            float stored = texture(shadowMapDepth, vec3(sp.xy, float(L))).r;
            float manual = sp.z <= stored ? 1.0 : 0.0;
            dbg = vec3(hw, manual, stored);
        }
        else if (term == 13) {
            // The cross-fade weight itself: black = pure current cascade, white = fully the next
            // one. Shows exactly where the transition band sits on screen.
            int cc = pc.cascadeCount > 0 ? pc.cascadeCount : int(cascadedShadow.color_and_misc.w + 0.5);
            float vz = frustumViewZ(FragPos);
            int L = clamp(cascadeForViewZ(vz, cc), 0, cc - 1);
            while (L + 1 < cc && !shadowUvOk(worldToShadowUv(L, FragPos))) L += 1;
            float bl = 0.0;
            if (L + 1 < cc) {
                float sp = cascadeSplit(L);
                float pv = L > 0 ? cascadeSplit(L - 1) : 0.0;
                float bd = max((sp - pv) * 0.14, 0.25);
                bl = clamp((vz - (sp - bd)) / bd, 0.0, 1.0);
            }
            dbg = vec3(bl);
        }
        else if (term == 12) {
            // Receiver depth minus stored occluder depth, amplified. >0.5 means the receiver sits
            // behind an occluder and should be shadowed; 0.5 means nothing is stored above it.
            int cc = pc.cascadeCount > 0 ? pc.cascadeCount : int(cascadedShadow.color_and_misc.w + 0.5);
            int L = clamp(cascadeForViewZ(frustumViewZ(FragPos), cc), 0, cc - 1);
            while (L + 1 < cc && !shadowUvOk(worldToShadowUv(L, FragPos))) L += 1;
            vec3 sp = worldToShadowUv(L, FragPos);
            float blocker = texture(shadowMapDepth, vec3(sp.xy, float(L))).r;
            dbg = vec3((sp.z - blocker) * 100.0 + 0.5);
        }
        else if (term == 11) {
            // Green where the selected cascade's UV+depth are inside [0,1] (a shadow lookup can
            // happen at all), red where they are not and the test silently returns "lit".
            int cc = pc.cascadeCount > 0 ? pc.cascadeCount : int(cascadedShadow.color_and_misc.w + 0.5);
            int L = clamp(cascadeForViewZ(frustumViewZ(FragPos), cc), 0, cc - 1);
            while (L + 1 < cc && !shadowUvOk(worldToShadowUv(L, FragPos))) L += 1;
            vec3 sp = worldToShadowUv(L, FragPos);
            dbg = shadowUvOk(sp) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        }
        else if (term == 10) {
            // Chosen cascade, as a step per level: 0 black, then 0.25 / 0.5 / 0.75 / 1.0.
            int cc = pc.cascadeCount > 0 ? pc.cascadeCount : int(cascadedShadow.color_and_misc.w + 0.5);
            int L = clamp(cascadeForViewZ(frustumViewZ(FragPos), cc), 0, cc - 1);
            while (L + 1 < cc && !shadowUvOk(worldToShadowUv(L, FragPos))) L += 1;
            dbg = vec3(float(L + 1) / float(max(cc, 1)));
        }
        FragColor = vec4(dbg, 1.0);
        return;
    }

    float skin = sfeSkinSss(albedo, metallic);
    float sssW = max(sssAmt, skin);
    vec3 lighting = ambient + Lo;
    if (sssW > 0.05 && metallic < 0.25 && clearcoat < 0.5) {
        float wrap = max(pc.padLook0, 0.0);
        float str = max(pc.padLook1, 0.0);
        float nDotL = dot(Normal, Lsun);
        float trans = max(-nDotL, 0.0);
        float wrapL = clamp((nDotL + wrap) / max(1.0 + wrap, 1e-3), 0.0, 1.0);
        vec3 scatter = albedo * vec3(0.55, 0.22, 0.16);
        vec3 sssL = scatter * cascadedShadow.color_and_misc.xyz * 3.14159265
            * (trans * 0.9 + wrapL * 0.25) * sssW * str;
        lighting += sssL * mix(shadowFactor, 1.0, trans * 0.5);
    }

    const float outlines = texture(gOutlines, TexCoords).b;
    vec3 fogColor = pc.ambientColor * 0.15 + vec3(0.08, 0.10, 0.14);
    float distFog = smoothstep(pc.fogStart, pc.drawDistance, Depth);
    float fogFactor = clamp(distFog * 0.45, 0.0, 0.35);
    vec3 color = mix(lighting, fogColor, fogFactor);
    if (dot(nSample, nSample) < 0.01) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    color += vec3(0.0, 0.78, 0.86) * outlines * 0.8;
    FragColor = vec4(color, 1.0);
}

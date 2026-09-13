#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D gPosition;
layout(SFE_SET(0, 1)) uniform sampler2D gNormal;
layout(SFE_SET(0, 2)) uniform sampler2D gAlbedoSpec;
layout(SFE_SET(0, 4)) uniform sampler2D sceneColor;

layout(SFE_SET(0, 3), std140) uniform SsrParams {
    vec3 viewPos;
    float maxDistance;
    vec2 resolution;
    float thickness;
    float stride;
} params;

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

const int STEPS = 32;
const int REFINE = 6;

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float screenFade(vec2 uv) {
    vec2 fade = smoothstep(0.0, 0.12, uv) * smoothstep(0.0, 0.12, 1.0 - uv);
    return fade.x * fade.y;
}

float viewZ(vec3 worldPos) {
    return -(matrices.view * vec4(worldPos, 1.0)).z;
}

bool project(vec3 worldPos, out vec2 uv) {
    vec4 clip = matrices.PV * vec4(worldPos, 1.0);
    if (clip.w <= 1e-4) {
        return false;
    }
    uv = clip.xy / clip.w * 0.5 + 0.5;
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

bool closerThanSurface(vec3 rayPos, vec3 sampled, float thickness) {
    float dz = viewZ(rayPos) - viewZ(sampled);
    return dz > 0.0 && dz < thickness;
}

void main() {
    vec4 lit = texture(sceneColor, TexCoords);
    vec4 posSample = texture(gPosition, TexCoords);
    vec3 worldPos = posSample.rgb;
    if (dot(worldPos, worldPos) < 1e-6) {
        FragColor = lit;
        return;
    }

    vec4 albedoSample = texture(gAlbedoSpec, TexCoords);
    vec3 albedo = albedoSample.rgb;
    float roughness = clamp(albedoSample.a, 0.04, 1.0);
    vec4 pbr = sfeGBufferPbr(posSample.a);
    float metallic = pbr.x;
    float specFactor = pbr.y;
    if (roughness > 0.72) {
        FragColor = lit;
        return;
    }

    vec3 N = normalize(texture(gNormal, TexCoords).rgb);
    vec3 V = normalize(params.viewPos - worldPos);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    vec3 R = reflect(-V, N);
    if (dot(R, N) < 0.02) {
        FragColor = lit;
        return;
    }

    vec3 F0 = mix(vec3(0.04 * specFactor), albedo, metallic);
    vec3 F = fresnelSchlick(NdotV, F0);
    float gloss = 1.0 - roughness;
    vec3 specW = F * (gloss * gloss);
    if (max(specW.r, max(specW.g, specW.b)) < 0.018) {
        FragColor = lit;
        return;
    }

    float maxDist = max(params.maxDistance, 1.0);
    float stepLen = max(maxDist / float(STEPS), 0.08);
    stepLen *= mix(0.96, 1.04, ign(gl_FragCoord.xy));
    float thickness = max(params.thickness, stepLen * 1.25);

    vec3 ray = worldPos + R * (0.08 + ign(gl_FragCoord.yx) * 0.04);
    vec3 prev = ray;
    vec2 hitUV = TexCoords;
    bool hit = false;

    for (int i = 0; i < STEPS; ++i) {
        prev = ray;
        ray += R * stepLen;
        if (distance(worldPos, ray) > maxDist) {
            break;
        }

        vec2 uv;
        if (!project(ray, uv)) {
            break;
        }

        vec3 sampled = texture(gPosition, uv).rgb;
        if (dot(sampled, sampled) < 1e-6) {
            continue;
        }
        if (!closerThanSurface(ray, sampled, thickness)) {
            continue;
        }

        hit = true;
        hitUV = uv;
        vec3 lo = prev;
        vec3 hi = ray;
        for (int k = 0; k < REFINE; ++k) {
            vec3 mid = mix(lo, hi, 0.5);
            vec2 midUV;
            if (!project(mid, midUV)) {
                break;
            }
            vec3 midSampled = texture(gPosition, midUV).rgb;
            if (dot(midSampled, midSampled) > 1e-6 && closerThanSurface(mid, midSampled, thickness)) {
                hi = mid;
                hitUV = midUV;
            } else {
                lo = mid;
            }
        }
        break;
    }

    if (!hit) {
        FragColor = lit;
        return;
    }

    float uvLen = length((hitUV - TexCoords) * max(params.resolution, vec2(1.0)));
    if (uvLen < 2.5) {
        FragColor = lit;
        return;
    }

    vec3 hitPos = texture(gPosition, hitUV).rgb;
    vec3 hitN = normalize(texture(gNormal, hitUV).rgb);
    if (dot(hitN, -R) < 0.05) {
        FragColor = lit;
        return;
    }

    vec3 reflected = texture(sceneColor, hitUV).rgb;
    float travel = clamp(distance(worldPos, hitPos) / maxDist, 0.0, 1.0);
    vec3 vis = specW * screenFade(hitUV) * (1.0 - travel * travel);
    FragColor = vec4(mix(lit.rgb, reflected, clamp(vis, 0.0, 1.0)), lit.a);
}

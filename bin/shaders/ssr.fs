#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D sceneDepth;
layout(SFE_SET(0, 6)) uniform sampler2D gMaterial;
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
    SFE_SHARED_MATRICES
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

// The view distance of what the scene shows at `uv`, straight from the depth buffer: the TAA jitter only moves x and
// y, so the jittered projection's depth terms are the plain ones. Negative where nothing was drawn.
float sceneViewZ(vec2 uv) {
    float depth = textureLod(sceneDepth, uv, 0.0).r;
    if (depth >= 1.0) {
        return -1.0;
    }
    return matrices.projection[3][2] / ((depth * 2.0 - 1.0) + matrices.projection[2][2]);
}

bool project(vec4 clip, out vec2 uv) {
    if (clip.w <= 1e-4) {
        return false;
    }
    uv = clip.xy / clip.w * 0.5 + 0.5;
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

bool closerThanSurface(float rayViewZ, float sampledViewZ, float thickness) {
    float dz = rayViewZ - sampledViewZ;
    return dz > 0.0 && dz < thickness;
}

void main() {
    vec4 lit = texture(sceneColor, TexCoords);
    // Roughness first: in a forest almost nothing is glossy enough, and that is one texel against a march.
    vec4 albedoSample = texture(gAlbedoSpec, TexCoords);
    float roughness = clamp(albedoSample.a, 0.04, 1.0);
    if (roughness > 0.72) {
        FragColor = lit;
        return;
    }
    vec3 worldPos = sfeScenePosition(sceneDepth, TexCoords, matrices.invProjection, matrices.invView);
    if (dot(worldPos, worldPos) < 1e-6) {
        FragColor = lit;
        return;
    }
    vec3 albedo = albedoSample.rgb;
    vec4 pbr = texture(gMaterial, TexCoords);
    float metallic = pbr.x;
    float specFactor = pbr.y;

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

    // The march by its distance along R: the ray's clip position and view distance are linear in it, so each step is
    // additions and one depth fetch instead of five matrix products.
    float start = 0.08 + ign(gl_FragCoord.yx) * 0.04;
    vec4 clipAt = matrices.PV * vec4(worldPos + R * start, 1.0);
    vec4 clipAlong = matrices.PV * vec4(R, 0.0);
    float viewZAt = -(matrices.view * vec4(worldPos + R * start, 1.0)).z;
    float viewZAlong = -(matrices.view * vec4(R, 0.0)).z;
    float along = 0.0;
    vec2 hitUV = TexCoords;
    bool hit = false;

    for (int i = 0; i < STEPS; ++i) {
        float before = along;
        along += stepLen;
        if (start + along > maxDist) {
            break;
        }

        vec2 uv;
        if (!project(clipAt + clipAlong * along, uv)) {
            break;
        }

        float sampledZ = sceneViewZ(uv);
        if (sampledZ < 0.0) {
            continue;
        }
        if (!closerThanSurface(viewZAt + viewZAlong * along, sampledZ, thickness)) {
            continue;
        }

        hit = true;
        hitUV = uv;
        float lo = before;
        float hi = along;
        for (int k = 0; k < REFINE; ++k) {
            float mid = 0.5 * (lo + hi);
            vec2 midUV;
            if (!project(clipAt + clipAlong * mid, midUV)) {
                break;
            }
            float midZ = sceneViewZ(midUV);
            if (midZ >= 0.0 && closerThanSurface(viewZAt + viewZAlong * mid, midZ, thickness)) {
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

    vec3 hitPos = sfeScenePosition(sceneDepth, hitUV, matrices.invProjection, matrices.invView);
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

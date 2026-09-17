#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D sceneHdr;
layout(SFE_SET(0, 1)) uniform sampler2D sceneDepth;
layout(SFE_SET(0, 2)) uniform sampler2D gAlbedoSpec;
layout(SFE_SET(0, 3)) uniform sampler2D gNormal;

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

layout(SFE_SET(0, 15), std140) uniform PushConstants {
    vec3 lightDir;
    float contactStrength;
    vec3 lightColor;
    float giStrength;
    vec3 viewPos;
    float maxDistance;
    vec2 resolution;
    float thickness;
    float giRadius;
} params;

const int CONTACT_STEPS = 16;
const int GI_SAMPLES = 8;
const float GOLDEN = 2.399963229728653;

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

float viewZ(vec3 world) {
    return -(matrices.view * vec4(world, 1.0)).z;
}

bool project(vec3 world, out vec2 uv) {
    vec4 clip = matrices.PV * vec4(world, 1.0);
    if (clip.w <= 1e-4) {
        return false;
    }
    uv = clip.xy / clip.w * 0.5 + 0.5;
    return uv.x > 0.001 && uv.x < 0.999 && uv.y > 0.001 && uv.y < 0.999;
}

vec2 vogel(int i, int n, float rot) {
    float r = sqrt((float(i) + 0.5) / float(n));
    float a = float(i) * GOLDEN + rot;
    return vec2(cos(a), sin(a)) * r;
}

float contactShadow(vec3 world, vec3 N, vec3 L, float noise) {
    float nDotL = dot(N, L);
    if (nDotL < 0.02) {
        return 0.0;
    }
    float vz = max(viewZ(world), 0.05);
    float maxDist = params.maxDistance;
    float thick = params.thickness * (1.0 + vz * 0.04);
    float occ = 0.0;
    for (int i = 0; i < CONTACT_STEPS; ++i) {
        float t = (float(i) + noise) / float(CONTACT_STEPS);
        t = t * t;
        vec3 rayPos = world + L * (0.004 * vz + t * maxDist);
        vec2 uv;
        if (!project(rayPos, uv)) {
            break;
        }
        vec3 sampled = sfeScenePosition(sceneDepth, uv, matrices.invProjection, matrices.invView);
        if (dot(sampled, sampled) < 1e-6) {
            continue;
        }
        float dz = viewZ(rayPos) - viewZ(sampled);
        if (dz > 0.0 && dz < thick) {
            float fade = 1.0 - t;
            occ = max(occ, fade * smoothstep(thick, 0.0, dz));
        }
    }
    return occ * smoothstep(0.02, 0.22, nDotL);
}

vec3 screenGI(vec3 world, vec3 N, vec3 albedo, float noise) {
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    float radius = params.giRadius;
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = 0; i < GI_SAMPLES; ++i) {
        vec2 vd = vogel(i, GI_SAMPLES, noise * 6.2831853);
        vec3 dir = normalize(T * vd.x + B * vd.y + N * 0.45);
        float dist = radius * (0.2 + 0.8 * length(vd));
        vec3 probe = world + dir * dist;
        vec2 uv;
        if (!project(probe, uv)) {
            continue;
        }
        vec3 sampledPos = sfeScenePosition(sceneDepth, uv, matrices.invProjection, matrices.invView);
        if (dot(sampledPos, sampledPos) < 1e-6) {
            continue;
        }
        vec3 delta = sampledPos - world;
        float dist2 = dot(delta, delta);
        if (dist2 < 1e-5 || dist2 > radius * radius * 4.0) {
            continue;
        }
        vec3 ldir = delta * inversesqrt(dist2);
        vec3 sampledN = normalize(texture(gNormal, uv).xyz);
        float receive = max(dot(N, ldir), 0.0);
        float send = max(dot(sampledN, -ldir), 0.0);
        if (receive * send < 0.02) {
            continue;
        }
        float att = 1.0 / (1.0 + dist2 * 6.0);
        vec3 sampledCol = texture(sceneHdr, uv).rgb;
        vec3 sampledAlb = texture(gAlbedoSpec, uv).rgb;
        float w = receive * send * att;
        acc += sampledCol * (0.55 + 0.45 * sampledAlb) * w;
        wsum += w;
    }
    if (wsum < 1e-4) {
        return vec3(0.0);
    }
    return (acc / wsum) * albedo * params.lightColor;
}

void main() {
    vec3 scene = texture(sceneHdr, TexCoords).rgb;
    vec3 world = sfeScenePosition(sceneDepth, TexCoords, matrices.invProjection, matrices.invView);
    if (dot(world, world) < 1e-6) {
        FragColor = vec4(scene, 1.0);
        return;
    }

    vec3 N = normalize(texture(gNormal, TexCoords).xyz);
    vec3 albedo = texture(gAlbedoSpec, TexCoords).rgb;
    vec3 L = normalize(-params.lightDir);
    float noise = ign(gl_FragCoord.xy);

    float contact = contactShadow(world, N, L, noise);
    float shadowMul = mix(1.0, 0.55, clamp(contact * params.contactStrength, 0.0, 1.0));
    vec3 gi = screenGI(world, N, albedo, noise) * params.giStrength * mix(1.0, 0.35, contact);

    FragColor = vec4(scene * shadowMul + gi, 1.0);
}

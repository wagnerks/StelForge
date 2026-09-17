#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D sceneHdr;
layout(SFE_SET(0, 1)) uniform sampler2D sceneDepth;
layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

layout(SFE_SET(0, 15), std140) uniform PushConstants {
    vec3 viewPos;
    float focusDistance;
    float focusRange;
    float maxBlur;
    vec2 resolution;
} params;

const vec2 kPoisson[8] = vec2[](
    vec2(-0.707,  0.707),
    vec2( 0.000, -1.000),
    vec2( 1.000,  0.000),
    vec2(-0.866, -0.500),
    vec2( 0.866, -0.500),
    vec2(-0.500,  0.866),
    vec2( 0.500,  0.866),
    vec2( 0.000,  0.500)
);

float cocAt(vec3 world) {
    float dist = (dot(world, world) < 1e-4)
        ? 1.0e4
        : distance(world, params.viewPos);
    float t = abs(dist - params.focusDistance) / max(params.focusRange, 0.01);
    return smoothstep(0.45, 1.0, t);
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(sceneHdr, 0));
    vec3 centerW = sfeScenePosition(sceneDepth, TexCoords, matrices.invProjection, matrices.invView);
    float coc = cocAt(centerW);
    vec3 center = texture(sceneHdr, TexCoords).rgb;
    float radius = coc * max(params.maxBlur, 0.0);
    if (radius < 0.45) {
        FragColor = vec4(center, 1.0);
        return;
    }

    vec3 acc = center;
    float wsum = 1.0;
    for (int i = 0; i < 8; ++i) {
        vec2 uv = clamp(TexCoords + kPoisson[i] * texel * radius, vec2(0.001), vec2(0.999));
        vec3 sampleW = sfeScenePosition(sceneDepth, uv, matrices.invProjection, matrices.invView);
        float gap = distance(sampleW, centerW);
        if (gap > 2.5) {
            continue;
        }
        float w = 0.25 + 0.75 * cocAt(sampleW);
        acc += texture(sceneHdr, uv).rgb * w;
        wsum += w;
    }
    FragColor = vec4(acc / max(wsum, 1e-4), 1.0);
}

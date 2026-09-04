#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D sceneHdr;
layout(SFE_SET(0, 1)) uniform sampler2D historyColor;
layout(SFE_SET(0, 2)) uniform sampler2D worldPos;

layout(SFE_SET(0, 15), std140) uniform PushConstants {
    mat4 prevPV;
    vec2 resolution;
    float blend;
    int reset;
} params;

vec3 neighborhoodClamp(vec3 history, vec2 texel, out float contrast) {
    vec3 aabbMin = vec3(1e10);
    vec3 aabbMax = vec3(-1e10);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 c = texture(sceneHdr, TexCoords + vec2(x, y) * texel).rgb;
            aabbMin = min(aabbMin, c);
            aabbMax = max(aabbMax, c);
        }
    }
    contrast = length(aabbMax - aabbMin);
    return clamp(history, aabbMin, aabbMax);
}

void main() {
    vec3 current = texture(sceneHdr, TexCoords).rgb;
    if (params.reset != 0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec3 world = texture(worldPos, TexCoords).rgb;
    vec2 prevUV = TexCoords;
    if (dot(world, world) > 1e-4) {
        vec4 prevClip = params.prevPV * vec4(world, 1.0);
        if (prevClip.w > 1e-4) {
            prevUV = prevClip.xy / prevClip.w * 0.5 + 0.5;
        }
    }

    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec2 texel = 1.0 / vec2(textureSize(sceneHdr, 0));
    float contrast = 0.0;
    vec3 history = neighborhoodClamp(texture(historyColor, prevUV).rgb, texel, contrast);
    float motion = length((TexCoords - prevUV) * vec2(textureSize(sceneHdr, 0)));
    float currentW = mix(1.0 - params.blend, 0.45, clamp(motion * 0.08 + contrast * 0.12, 0.0, 1.0));
    vec3 color = mix(history, current, currentW);
    FragColor = vec4(color, 1.0);
}

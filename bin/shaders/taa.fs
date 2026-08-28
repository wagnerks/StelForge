#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneHdr;
layout(binding = 1) uniform sampler2D historyColor;
layout(binding = 2) uniform sampler2D worldPos;

layout(std140, binding = 15) uniform PushConstants {
    mat4 prevPV;
    vec2 resolution;
    float blend;
    int reset;
} params;

vec3 neighborhoodClamp(vec3 history, vec2 texel) {
    vec3 aabbMin = vec3(1e10);
    vec3 aabbMax = vec3(-1e10);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 c = texture(sceneHdr, TexCoords + vec2(x, y) * texel).rgb;
            aabbMin = min(aabbMin, c);
            aabbMax = max(aabbMax, c);
        }
    }
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
    vec3 history = neighborhoodClamp(texture(historyColor, prevUV).rgb, texel);
    float motion = length((TexCoords - prevUV) * vec2(textureSize(sceneHdr, 0)));
    float currentW = mix(1.0 - params.blend, 0.28, clamp(motion * 0.08, 0.0, 1.0));
    vec3 color = mix(history, current, currentW);
    FragColor = vec4(color, 1.0);
}

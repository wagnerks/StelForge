#version 450 core
#include "sfe.glsl"

// The raw-depth half of rhi_test_shadow_compare.fs: the same UVs through a plain sampler2DArray,
// as deferred_shading.fs reads the cascade depth directly. Same channel layout, so the
// test decodes both the same way.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2DArray shadowDepth;

layout(SFE_SET(0, 1), std140) uniform TestShadowBlock {
    vec4 scaleOffset; // xy scale, zw offset
    vec4 sizeRef;     // xy target size; zw unused
} block;

void main() {
    vec2 px = floor(vUv * block.sizeRef.xy);
    vec2 uv = block.scaleOffset.zw + (px + 0.5) / block.sizeRef.xy * block.scaleOffset.xy;
    float depth = texture(shadowDepth, vec3(uv, 1.0)).r;
    float wide = floor(depth * 65535.0 + 0.5);
    fragColor = vec4(depth, floor(wide / 256.0) / 255.0, mod(wide, 256.0) / 255.0, px.y / 255.0);
}

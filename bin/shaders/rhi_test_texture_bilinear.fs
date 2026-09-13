#version 450 core
#include "sfe.glsl"

// Bilinear sampling at deterministic sub-texel UVs. The UV is built from the fragment's integer
// pixel index, not from the interpolated vUv directly, so the CPU can rebuild it bit for bit.
// Alpha carries that pixel's row index, which makes the result independent of the readback's
// row order: the test files each pixel by the row it reports, not by where it landed.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2D testTexture;

layout(SFE_SET(0, 1), std140) uniform TestBilinearBlock {
    vec4 scaleOffset; // xy scale, zw offset
    vec4 size;        // xy target size; zw unused
} block;

void main() {
    vec2 px = floor(vUv * block.size.xy);
    vec2 uv = block.scaleOffset.zw + (px + 0.5) / block.size.xy * block.scaleOffset.xy;
    fragColor = vec4(texture(testTexture, uv).rgb, px.y / 255.0);
}

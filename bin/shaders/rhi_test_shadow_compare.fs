#version 450 core
#include "sfe.glsl"

// Hardware PCF the way deferred_shading.fs samples the cascade map: texture() on a
// sampler2DArrayShadow, layer 1, at deterministic sub-texel UVs built from the pixel index (see
// rhi_test_texture_bilinear.fs). R is the result as an 8-bit target stores it, which is what the
// lighting pass sees; G and B carry the same result as 16 bits so a weight difference smaller
// than 1/255 still shows. Alpha is the row index, for row-order independent comparison.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2DArrayShadow shadowMap;

layout(SFE_SET(0, 1), std140) uniform TestShadowBlock {
    vec4 scaleOffset; // xy scale, zw offset
    vec4 sizeRef;     // xy target size, z reference depth, w unused
} block;

void main() {
    vec2 px = floor(vUv * block.sizeRef.xy);
    vec2 uv = block.scaleOffset.zw + (px + 0.5) / block.sizeRef.xy * block.scaleOffset.xy;
    float lit = texture(shadowMap, vec4(uv, 1.0, block.sizeRef.z));
    float wide = floor(lit * 65535.0 + 0.5);
    fragColor = vec4(lit, floor(wide / 256.0) / 255.0, mod(wide, 256.0) / 255.0, px.y / 255.0);
}

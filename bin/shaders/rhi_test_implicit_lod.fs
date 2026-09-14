#version 450 core
#include "sfe.glsl"

// Implicit-LOD sampling and the derivatives behind it, on the quad rhi_test_perspective_quad.vs
// draws. select.x picks what is written:
//   0 texture(), as g_buffer.fs samples albedo
//   1 textureGrad() with this fragment's own dFdx/dFdy passed explicitly
//   2 textureLod() at textureQueryLod().x
//   10.. one float, written as its exact bits across RGBA8 (see rhi_test_float_bits.fs), so any
//        difference below 1/255 and any sign flip show: uv, dFdx(uv), dFdy(uv), fwidth(uv),
//        textureQueryLod().xy.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2D testTexture;

layout(SFE_SET(0, 1), std140) uniform TestImplicitLodBlock {
    vec4 select;
} block;

void main() {
    int q = int(block.select.x);
    vec2 dx = dFdx(vUv);
    vec2 dy = dFdy(vUv);
    vec2 lod = textureQueryLod(testTexture, vUv);
    if (q == 0) {
        fragColor = texture(testTexture, vUv);
        return;
    }
    if (q == 1) {
        fragColor = textureGrad(testTexture, vUv, dx, dy);
        return;
    }
    if (q == 2) {
        fragColor = textureLod(testTexture, vUv, lod.x);
        return;
    }
    vec2 fw = fwidth(vUv);
    float v[10] = float[10](vUv.x, vUv.y, dx.x, dx.y, dy.x, dy.y, fw.x, fw.y, lod.x, lod.y);
    uint b = floatBitsToUint(v[q - 10]);
    uvec4 bytes = uvec4(b & 255u, (b >> 8u) & 255u, (b >> 16u) & 255u, b >> 24u);
    fragColor = vec4(bytes) / 255.0;
}

#version 450 core
#include "sfe.glsl"

// Three explicit lods of one texture at one UV, packed into three channels of one pixel, so a
// single draw says what each mip level holds and a level that did not arrive shows up as a channel
// carrying another level's value.
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2D testTexture;

// std140 for the same reason rhi_test_sampler_uv.fs uses it: OpenGL's default block layout is
// implementation defined, so the bytes the test uploads would not have to land on uv on both
// backends without it.
layout(SFE_SET(0, 1), std140) uniform TestLodBlock {
    vec4 uv; // xy is the UV to sample; zw unused
} block;

void main() {
    // textureLod, not texture: the UV is the same for every fragment, so the implicit derivatives
    // are zero and every implicit lookup would be lod 0. The lod has to be stated to be tested.
    fragColor = vec4(
        textureLod(testTexture, block.uv.xy, 0.0).r,
        textureLod(testTexture, block.uv.xy, 1.0).r,
        textureLod(testTexture, block.uv.xy, 2.0).r,
        1.0);
}

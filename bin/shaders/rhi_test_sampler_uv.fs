#version 450 core
#include "sfe.glsl"

// Like rhi_test_sampler.fs, but the UV comes from a uniform instead of from the interpolator.
// Sampler state can only be told apart at UVs the test chooses exactly — on a texel boundary, or
// outside [0, 1] where the address mode decides the answer — and the interpolated vUv reaches
// neither: it covers [0, 1] at the pixel centres of the target and nothing else.
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2D testTexture;

// std140 for the same reason rhi_test_uniform.fs uses it: OpenGL's default block layout is
// implementation defined, so the bytes the test uploads would not have to land on uv on both
// backends without it.
layout(SFE_SET(0, 1), std140) uniform TestSampleUvBlock {
    vec4 uv; // xy is the UV to sample; zw unused
} block;

void main() {
    // The UV is the same for every fragment, so the derivatives are zero and the lookup is lod 0 on
    // both backends — no mip selection creeps into a test about filter and address mode.
    fragColor = texture(testTexture, block.uv.xy);
}

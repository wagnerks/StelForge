#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 fragColor;

// Two descriptor sets read by one draw, each at its own binding number. On Vulkan the set index is
// half of the address and both are honoured; on OpenGL SFE_SET drops the set index entirely, so the
// two blocks are told apart by their binding numbers alone. Distinct binding numbers are therefore
// the only shape both backends can express. std140 for the reason rhi_test_uniform.fs gives.
layout(SFE_SET(0, 0), std140) uniform TestSetZeroBlock {
    vec4 color;
} setZero;

layout(SFE_SET(1, 1), std140) uniform TestSetOneBlock {
    vec4 color;
} setOne;

void main() {
    // Two channels from each set, so both are pinned and a set that never arrived leaves zeros in
    // its own pair of channels rather than being mistaken for a wrong value in the other.
    fragColor = vec4(setZero.color.rg, setOne.color.rg);
}

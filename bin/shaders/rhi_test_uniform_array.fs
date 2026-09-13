#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 fragColor;

// An array of uniform blocks, which is the buffer half of the same array-descriptor question. On
// Vulkan this is one binding with descriptorCount 2; on OpenGL the GLSL rule for a block array with
// an explicit binding is that element i takes binding + i, which is exactly the mapping
// execBindDescriptorSets applies. std140 for the reason rhi_test_uniform.fs gives: OpenGL's default
// block layout is implementation defined, so without it the bytes the test uploads would not have to
// land on the same member on both backends.
layout(SFE_SET(0, 4), std140) uniform TestUniformArrayBlock {
    vec4 color;
} blocks[2];

void main() {
    // Two channels from each element, so both elements are pinned and a missing second block cannot
    // be mistaken for a wrong value in the first.
    fragColor = vec4(blocks[0].color.rg, blocks[1].color.rg);
}

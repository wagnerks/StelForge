#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 fragColor;

// One block, and deliberately in set 1 rather than set 0: this is the shader for the firstSet probe,
// where a single set is bound at index 1 against a pipeline layout that declares both. Nothing here
// reads set 0, so leaving it unbound is not a descriptor the draw statically uses and the frame stays
// valid on Vulkan. On OpenGL SFE_SET drops the set index and only binding 1 remains. std140 for the
// reason rhi_test_uniform.fs gives.
layout(SFE_SET(1, 1), std140) uniform TestSetOneOnlyBlock {
    vec4 color;
} setOne;

void main() {
    fragColor = setOne.color;
}

#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 fragColor;

// The same block, at the same binding and in the same std430 layout, as rhi_test_compute.cs writes.
// This shader exists only because mapBuffer cannot read a storage buffer on both backends (see the
// comment in RhiComputeTests.cpp): it stages the buffer through a colour attachment instead, so the
// bytes the compute shader wrote come back to the host through readbackTexture.
layout(SFE_SET(0, 0), std430) readonly buffer ComputeResult {
    uint values[];
};

void main() {
    // gl_FragCoord.x, not vUv: the target is one pixel tall, so the axis whose origin the two
    // backends disagree about does not take part at all, and X runs left to right on both. A pixel
    // centre is exactly i + 0.5, so the truncation names element i and no rounding decides it.
    int idx = int(gl_FragCoord.x);

    // The four bytes of the element, least significant first, each landing on one whole UNORM8
    // step so the host can reassemble the exact uint32 the compute shader wrote.
    uint v = values[idx];
    fragColor = vec4(
        float(v & 0xFFu),
        float((v >> 8) & 0xFFu),
        float((v >> 16) & 0xFFu),
        float((v >> 24) & 0xFFu)) / 255.0;
}

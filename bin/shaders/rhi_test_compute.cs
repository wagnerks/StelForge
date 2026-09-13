#version 450 core
#include "sfe.glsl"

// Four invocations per group, so a dispatch of N groups covers exactly 4N elements and a wrong
// group count leaves a visible tail rather than a plausible-looking short answer.
layout(local_size_x = 4) in;

// std430, which is what every storage block in this engine uses: an array of uint packs at a
// stride of 4 bytes and lines up with the plain C++ uint32_t array the test uploads and reads back.
// Binding 0, not 15, which SFE_PUSH_BLOCK reserves on OpenGL.
layout(SFE_SET(0, 0), std430) writeonly buffer Result {
    uint values[];
};

void main() {
    // Each invocation writes its own slot, and writes a value derived from its own id: a shader
    // that ran once, or ran with every invocation seeing the same id, produces a different buffer
    // rather than the right one. The high half is a constant marker so that every one of the four
    // bytes the host reassembles is non-zero and a dropped channel is visible; the low half is
    // 1000 + 7i, which spans two bytes over the range dispatched here, so a wrong index moves more
    // than one of them.
    uint i = gl_GlobalInvocationID.x;
    values[i] = 0xA5A50000u | (1000u + i * 7u);
}

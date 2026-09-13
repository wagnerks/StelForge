#version 450 core
#include "sfe.glsl"

// Two bindings that have to advance at different rates. Binding 0 is the triangle itself and is
// read once per vertex; binding 1 is the per-instance record and must be read once per instance,
// so all three vertices of one copy share one offset and one colour. A divisor that came out as
// per-vertex would hand the three vertices three different offsets and tear the triangle apart.
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inInstanceOffset;
layout(location = 3) in vec4 inInstanceColor;

// flat: every vertex of an instance carries the same colour, so interpolation would only add a
// rounding question to a test that is about which record was fetched.
layout(location = 0) flat out vec4 vColor;

void main() {
    // The per-vertex colour is white, so the pixel reports the per-instance record's colour while
    // still proving binding 0 was read at all.
    vColor = inColor * inInstanceColor;
    gl_Position = vec4(inPos + inInstanceOffset, 0.0, 1.0);
}

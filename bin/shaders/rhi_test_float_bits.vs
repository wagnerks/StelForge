#version 450 core
#include "sfe.glsl"

// Full-screen triangle from the vertex index. The varying is an affine function of the corner so the
// interpolated value at each pixel is a non-trivial float, and it is written out raw by the fragment
// shader to compare the interpolators bit for bit.
SFE_PUSH_BLOCK FloatBitsPush {
    mat4 viewProj;
    vec4 seed;
    vec4 weights;
    vec4 uvScaleBias;
    vec4 select;
} pc;

layout(location = 0) out vec2 vUv;

void main() {
    vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vec2 p = corners[SFE_VERTEX_ID];
    vUv = p * pc.uvScaleBias.xy + pc.uvScaleBias.zw;
    gl_Position = vec4(p, 0.0, 1.0);
}

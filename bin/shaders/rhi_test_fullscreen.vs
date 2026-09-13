#version 450 core
#include "sfe.glsl"

// A full-screen triangle built out of the vertex index alone: no vertex buffer, and no push
// constants either. Descriptor tests want the descriptor set to be the only thing feeding the
// pipeline, so that a wrong pixel can only mean the set did not arrive.
layout(location = 0) out vec2 vUv;

void main() {
    vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vec2 p = corners[SFE_VERTEX_ID];
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}

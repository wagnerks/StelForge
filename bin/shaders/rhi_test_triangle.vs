#version 450 core
#include "sfe.glsl"

// Three vertices straight from the vertex index, so the conformance tests need no vertex buffer.
// The push block carries the corners, which lets one shader cover a full-screen triangle, a
// half-screen one and a depth-offset one without a shader per case.
SFE_PUSH_BLOCK TrianglePush {
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 color;
} tri;

void main() {
    vec4 corners[3] = vec4[3](tri.a, tri.b, tri.c);
    gl_Position = corners[SFE_VERTEX_ID];
}

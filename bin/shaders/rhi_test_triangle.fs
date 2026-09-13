#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 fragColor;

SFE_PUSH_BLOCK TrianglePush {
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 color;
} tri;

void main() {
    fragColor = tri.color;
}

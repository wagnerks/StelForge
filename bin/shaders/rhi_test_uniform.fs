#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 fragColor;

// std140 explicitly: OpenGL's default block layout is implementation defined, so the same bytes
// uploaded by the test would not have to land on the same members on both backends without it.
layout(SFE_SET(0, 0), std140) uniform TestUniformBlock {
    vec4 color;
} block;

void main() {
    fragColor = block.color;
}

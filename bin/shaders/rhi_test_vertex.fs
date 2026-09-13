#version 450 core
#include "sfe.glsl"

layout(location = 0) flat in vec4 vColor;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = vColor;
}

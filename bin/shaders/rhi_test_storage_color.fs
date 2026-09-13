#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0), std430) readonly buffer TestStorageColorBlock {
    vec4 color;
} block;

void main() {
    fragColor = block.color;
}

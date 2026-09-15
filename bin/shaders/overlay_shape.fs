#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;

SFE_PUSH_BLOCK OverlayShape {
    vec4 color;
} shape;

void main() {
    FragColor = shape.color;
}

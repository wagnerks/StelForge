#version 450 core
#include "sfe.glsl"

layout(location = 0) out vec4 gOutlines;

void main() {
    gOutlines = vec4(0.0, 1.0, 0.0, 1.0);
}

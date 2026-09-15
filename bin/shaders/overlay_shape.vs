#version 450 core
#include "sfe.glsl"
// Screen-space overlay geometry: the position is already in clip space, so nothing is transformed here.
layout(location = 0) in vec2 position;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
}

#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec4 vertex;
layout(location = 0) out vec2 TexCoords;

void main() {
    gl_Position = vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}

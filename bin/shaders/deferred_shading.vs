#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

layout(location = 0) out vec2 TexCoords;

void main() {
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}

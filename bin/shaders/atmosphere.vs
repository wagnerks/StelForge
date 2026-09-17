#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

layout(location = 0) out vec2 vNdc;

void main() {
    vNdc = aPos;
    // On the far plane: the depth test lets it through only where nothing was drawn.
    gl_Position = vec4(aPos, 1.0, 1.0);
}

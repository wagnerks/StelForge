#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec2 TexCoords;
layout(location = 0) out vec4 color;

SFE_PUSH_BLOCK PushConstants {
    vec4 drawColor;
} pc;

void main()
{
    color = pc.drawColor;
}

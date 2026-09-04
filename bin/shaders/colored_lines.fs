#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec3 texPos;

SFE_PUSH_BLOCK Push {
    vec4 color;
    mat4 PVM;
} pc;

void main()
{
    FragColor = pc.color;
}

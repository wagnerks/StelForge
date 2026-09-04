#version 450 core
#include "sfe.glsl"
layout (location = 0) in vec3 aPos;

layout(location = 0) out vec3 texPos;

SFE_PUSH_BLOCK Push {
    vec4 color;
    mat4 PVM;
} pc;

void main()
{
    texPos = aPos;
    gl_Position = pc.PVM * vec4(aPos, 1.0);
}

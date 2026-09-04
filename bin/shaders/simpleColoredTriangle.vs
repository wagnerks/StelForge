#version 450 core
#include "sfe.glsl"
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;

SFE_PUSH_BLOCK Push {
    vec4 viewPos;
    mat4 PVM;
} pc;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec4 fragColor;

void main()
{
    normal = aNormal;
    fragPos = aPos;
    fragColor = aColor;
    gl_Position = pc.PVM * vec4(aPos, 1.0);
}

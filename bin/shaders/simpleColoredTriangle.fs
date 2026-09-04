#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 fragColor;

SFE_PUSH_BLOCK Push {
    vec4 viewPos;
    mat4 PVM;
} pc;

void main()
{
    vec3 viewDir = normalize(pc.viewPos.xyz - fragPos);
    float illum = (dot(normal, viewDir) + 1.0) * 0.5;
    FragColor = vec4(fragColor.xyz * illum, fragColor.a);
}

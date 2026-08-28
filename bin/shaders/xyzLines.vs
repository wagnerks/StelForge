#version 430 core
layout (location = 0) in vec3 aPos;

out vec3 texPos;

layout(std140, binding = 15) uniform PushConstants {
    vec4 color;
    mat4 PVM;
} pc;

void main()
{
    texPos = aPos;
    gl_Position = pc.PVM * vec4(aPos, 1.0);
}

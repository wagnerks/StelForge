#version 330 core
in vec2 TexCoords;
out vec4 color;

layout (std140, binding = 15) uniform PushConstants {
    vec4 drawColor;
} pc;

void main()
{    
    color = pc.drawColor;
}
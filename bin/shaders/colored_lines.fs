#version 330 core
out vec4 FragColor;

in vec3 texPos;
layout(std140, binding = 15) uniform PushConstants {
    vec4 color;
} pc;

void main()
{ 
    FragColor = pc.color;
}

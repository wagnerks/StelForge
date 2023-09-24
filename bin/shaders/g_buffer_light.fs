#version 330 core

layout (location = 5) out vec4 gLights;


out vec4 FragColor;

void main()
{ 
    gLights.r = 1.0;
    gLights.a = 1.0;
}

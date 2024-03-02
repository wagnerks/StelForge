#version 330 core

layout (location = 0) out vec4 gOutlines;

void main()
{ 
    gOutlines.g = 1.0;
    gOutlines.a = 1.0;
}
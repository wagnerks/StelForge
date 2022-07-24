#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 PVM;
out vec3 texPos;

void main()
{
    texPos = aPos;
    gl_Position = PVM * vec4(aPos, 1.0); 
}  
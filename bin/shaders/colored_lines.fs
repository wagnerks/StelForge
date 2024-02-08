#version 330 core
out vec4 FragColor;

in vec3 texPos;

uniform vec4 color = vec4(1.f, 0.f, 0.f, 1.0);
void main()
{ 
    FragColor = color;
}

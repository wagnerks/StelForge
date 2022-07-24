#version 330 core
out vec4 FragColor;

in vec3 texPos;
void main()
{ 
    FragColor = vec4(texPos.x, texPos.y, texPos.z, 1.0);
}
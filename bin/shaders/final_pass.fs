#version 430 core
layout (location = 0) out vec4 gOutlines;

layout (binding = 0) uniform sampler2D finalTexture;

out vec4 FragColor;

in vec2 TexCoord;

void main()
{    
    FragColor = texture(finalTexture, TexCoord);
}
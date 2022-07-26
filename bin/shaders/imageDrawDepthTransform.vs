#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

out VS_OUT {
    mat4 FragPosLightSpace;
} vs_out;

uniform mat4 lightSpaceMatrix;

void main()
{
    vs_out.FragPosLightSpace = lightSpaceMatrix;

    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}
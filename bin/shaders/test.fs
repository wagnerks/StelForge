#version 430 core
out vec4 FragColor;


layout (location = 2) out vec4 gAlbedoSpec;

in vec3 texPos;
void main()
{
    gAlbedoSpec = vec4(gAlbedoSpec.xyz, 1.0);
}

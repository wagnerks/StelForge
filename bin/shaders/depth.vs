#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;

layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

void main()
{
    gl_Position = lightSpaceMatrix * model[gl_InstanceID] * vec4(aPos, 1.0);
}

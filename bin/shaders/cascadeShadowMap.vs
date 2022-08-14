#version 460 core
layout (location = 0) in vec3 aPos;

layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

void main()
{
    gl_Position = model[gl_InstanceID] * vec4(aPos, 1.0);
}

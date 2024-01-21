#version 410 core
layout (location = 0) in vec3 aPos;


layout(std140) uniform modelMatrices
{
    mat4 model[50];
};

uniform mat4 PV;

void main()
{
    gl_Position = PV * model[gl_InstanceID] * vec4(aPos, 1.0);
}
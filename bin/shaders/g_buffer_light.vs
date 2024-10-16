#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;

layout (location = 7) in uint entityIdx;

layout(std430, binding = 10) buffer modelMatrices
{
    mat4 model[];
};

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

void main()
{
    gl_Position = matrices.PV * model[entityIdx] * vec4(aPos, 1.0);
}

#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;

layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};
layout(std430, binding = 3) buffer gpuIndex
{
    uint entToGpuIdx[];
};
layout(std140, binding = 5) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

void main()
{
    gl_Position = matrices.PV * model[entToGpuIdx[gl_InstanceID]] * vec4(aPos, 1.0);
}

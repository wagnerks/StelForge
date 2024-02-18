#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(std430, binding = 2) buffer bonesMatrices
{
    mat4 bones[][MAX_BONES];
};

void main()
{
    mat4 BoneTransform = mat4(0.0f);
    bool withBones = false;
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        int boneIdx = aBoneIds[i];
        if(boneIdx == -1 || boneIdx >= MAX_BONES){
             continue;
        }
        withBones = true;
        BoneTransform += bones[gl_InstanceID][boneIdx] * aWeights[i];
    }
    if (!withBones){
        BoneTransform = mat4(1.f);
    }
    vec4 totalPosition = BoneTransform * vec4(aPos.xyz, 1.0);
    totalPosition /= totalPosition.w;

    gl_Position = matrices.PV * (model[gl_InstanceID] * totalPosition);
}
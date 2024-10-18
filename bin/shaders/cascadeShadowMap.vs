#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;
layout (location = 7) in uint entityIdx;

layout(std430, binding = 10) buffer modelMatrices
{
    mat4 model[];
};

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(std430, binding = 11) buffer bonesMatrices
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
        BoneTransform += bones[entityIdx][boneIdx] * aWeights[i];
    }
    if (!withBones){
        BoneTransform = mat4(1.f);
    }
    vec4 newPos = BoneTransform * vec4(aPos, 1.0);
    newPos /= newPos.w;
    gl_Position = model[entityIdx] * newPos;
}

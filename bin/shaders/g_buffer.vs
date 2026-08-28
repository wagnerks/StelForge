#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

out highp vec3 FragPos;
out vec2 TexCoords;
out mat3 TBN;


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

layout(std430, binding = 3) buffer gpuIndex
{
    uint entToGpuIdx[];
};

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

void main() {
    uint index = entToGpuIdx[gl_InstanceID];

    mat4 BoneTransform = mat4(0.0f);
    bool withBones = false;
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        int boneIdx = aBoneIds[i];
        if(boneIdx < 0 || boneIdx >= MAX_BONES){
             continue;
        }
        withBones = true;
        BoneTransform += bones[index][boneIdx] * aWeights[i];
    }
    if (!withBones){
        BoneTransform = mat4(1.f);
    }
    
    mat3 normalMatrix = mat3(model[index]) * mat3(BoneTransform);
    vec4 newPos = BoneTransform * vec4(aPos.xyz, 1.0);
    newPos /= newPos.w;
    vec4 worldPos = model[index] * vec4(newPos);

    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;
    
    TBN[0] = normalize(normalMatrix * aTangents);
    TBN[1] = normalize(normalMatrix * aBiTangents);
    TBN[2] = normalize(normalMatrix * aNormal);

    gl_Position = matrices.PV * worldPos;
}

#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

layout (location = 7) in uint entityIdx;

out highp vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out vec3 ViewPos;
out mat3 TBN;

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(std430, binding = 10) buffer modelMatricesStatic
{
    mat4 modelStatic[];
};

layout(std430, binding = 11) buffer bonesMatricesStatic
{
    mat4 bonesStatic[][MAX_BONES];
};

layout(std430, binding = 12) buffer isAnimatedData
{
    bool animated[];
};

void main() {
    mat4 BoneTransform = mat4(0.0f);
    bool withBones = false;
    
    int boneIdx = aBoneIds[0];
    if(boneIdx != -1 && boneIdx < MAX_BONES){
        withBones = true;
        BoneTransform += bonesStatic[entityIdx][boneIdx] * aWeights[0];
    }
   
    boneIdx = aBoneIds[1];
    if(boneIdx != -1 && boneIdx < MAX_BONES){
        withBones = true;
        BoneTransform += bonesStatic[entityIdx][boneIdx] * aWeights[1];
    }

    boneIdx = aBoneIds[2];
    if(boneIdx != -1 && boneIdx < MAX_BONES){
        withBones = true;
        BoneTransform += bonesStatic[entityIdx][boneIdx] * aWeights[2];
    }

    boneIdx = aBoneIds[3];
    if(boneIdx != -1 && boneIdx < MAX_BONES){
        withBones = true;
        BoneTransform += bonesStatic[entityIdx][boneIdx] * aWeights[3];
    }

    if (!withBones){
        BoneTransform = mat4(1.f);
    }

    mat3 normalMatrix = transpose(inverse(mat3(modelStatic[entityIdx]))) * transpose(inverse(mat3(BoneTransform)));
    vec4 newPos = BoneTransform * vec4(aPos.xyz, 1.0);
    newPos /= newPos.w;
    vec4 worldPos = modelStatic[entityIdx] * vec4(newPos);

    ViewPos = vec3(matrices.view * worldPos);

    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;

    TBN[0] = normalize(normalMatrix * aTangents);
    TBN[1] = normalize(normalMatrix * aBiTangents);
    TBN[2] = normalize(normalMatrix * aNormal);

    gl_Position = matrices.PV * worldPos;
}
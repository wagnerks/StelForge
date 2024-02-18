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
out vec3 Normal;
out vec3 ViewPos;
out mat3 TBN;

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

void main() {
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
    mat3 normalMatrix = transpose(inverse(mat3(model[gl_InstanceID]))) * transpose(inverse(mat3(BoneTransform)));
    vec4 newPos = BoneTransform * vec4(aPos.xyz, 1.0);
    newPos /= newPos.w;
    vec4 worldPos = model[gl_InstanceID] * vec4(newPos);

    ViewPos = vec3(matrices.view * worldPos);

    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;
    
    TBN[0] = normalize(normalMatrix * aTangents);
    TBN[1] = normalize(normalMatrix * aBiTangents);
    TBN[2] = normalize(normalMatrix * aNormal);

    gl_Position = matrices.PV * worldPos;
}
#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

layout (location = 7) in uint entityIdx;

uniform mat4 PV;
out vec3 texPos;

layout(std430, binding = 10) buffer modelMatrices
{
    mat4 model[];
};

layout(std430, binding = 11) buffer bonesMatrices
{
    mat4 bones[][100];
};
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

void main()
{
    bool noBones = true;
    mat4 BoneTransform = mat4(0.0f);
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        int boneIdx = aBoneIds[i];
        if(boneIdx == -1 || boneIdx >= MAX_BONES){
             continue;
        }
        noBones = false;

        BoneTransform += bones[entityIdx][boneIdx] * aWeights[i];
    }


    vec4 totalPosition;
    if (noBones) {
        totalPosition = vec4(aPos.xyz, 1.0);
    }
    else {
        totalPosition = BoneTransform * vec4(aPos.xyz, 1.0);
    }

    texPos = aPos;
    mat4 m = model[entityIdx];
    gl_Position = PV * m  * vec4(totalPosition + aNormal* 5.0,1.0); 
}  

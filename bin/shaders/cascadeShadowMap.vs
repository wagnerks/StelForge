#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

layout(std430, binding = 2) buffer bonesMatrices
{
    mat4 bones[][100];
};

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

void main()
{
   bool noBones = true;
    vec4 totalPosition = vec4(0.0f);
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        if(aBoneIds[i] == -1) {
             continue;
        }
        if(aBoneIds[i] >= MAX_BONES){
            totalPosition = vec4(aPos,1.0f);
            break;
        }
        noBones = false;
        vec4 localPosition = bones[gl_InstanceID][aBoneIds[i]] * vec4(aPos,1.0f);
        totalPosition += localPosition * aWeights[i];
        
        //vec3 localNormal = mat3(bones[gl_InstanceID][aBoneIds[i]]) * aNormal;
    }
    if (noBones){
        totalPosition.xyz = aPos;
    }

    gl_Position = model[gl_InstanceID] * vec4(totalPosition.xyz, 1.0);
}

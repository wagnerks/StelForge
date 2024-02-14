#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

uniform mat4 PV;
out vec3 texPos;

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
    vec4 totalPosition = vec4(0.0f);
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        if(aBoneIds[i] == -1) {
             continue;
        }
        if(aBoneIds[i] >= MAX_BONES){
            totalPosition = vec4(aPos,1.0f);
            break;
        }

        vec4 localPosition = bones[gl_InstanceID][aBoneIds[i]] * vec4(aPos,1.0f);
        totalPosition += localPosition * aWeights[i];
    }

    texPos = aPos;
    mat4 m = model[gl_InstanceID];
    gl_Position = PV * m  * vec4(totalPosition + aNormal* 5.0,1.0); 
}  

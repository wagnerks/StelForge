#version 450 core
#include "sfe.glsl"
#include "sfe_skin.glsl"
layout(location = 0) in vec3 aPos;
layout(location = 5) in ivec4 aBoneIds;
layout(location = 6) in vec4 aWeights;

layout(SFE_SET(0, 1), std430) readonly buffer modelMatrices {
    mat4 model[];
};

const int MAX_BONE_INFLUENCE = 4;

// Every armature's bones back to back; an instance's start at its boneOffset.
layout(SFE_SET(0, 2), std430) readonly buffer bonesMatrices {
    mat4 bones[];
};

#include "sfe_scene.glsl"

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

void main() {
    SfeInstance instance = sfeInstances[SFE_INSTANCE_ID];
    uint index = instance.entity;

    SfeSkin skin = sfeSkinNone();
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        int boneIdx = aBoneIds[i];
        if (boneIdx < 0) {
            continue;
        }
        sfeSkinAdd(skin, bones[instance.boneOffset + uint(boneIdx)], aWeights[i]);
    }
    mat4 BoneTransform = sfeSkinMatrix(skin);

    vec4 totalPosition = BoneTransform * vec4(aPos.xyz, 1.0);
    totalPosition /= totalPosition.w;
    gl_Position = matrices.PV * (model[index] * totalPosition);
}

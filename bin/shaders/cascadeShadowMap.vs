#version 450 core
#include "sfe.glsl"
#include "sfe_scene.glsl"
layout(location = 0) in vec3 aPos;
layout(location = 5) in ivec4 aBoneIds;
layout(location = 6) in vec4 aWeights;

layout(location = 3) flat out uint vMaterial;

layout(SFE_SET(0, 1), std430) readonly buffer modelMatrices {
    mat4 model[];
};

const int MAX_BONES = 256;
const int MAX_BONE_INFLUENCE = 4;

layout(SFE_SET(0, 2), std430) readonly buffer bonesMatrices {
    mat4 bones[][MAX_BONES];
};

void main() {
    SfeInstance instance = sfeInstances[SFE_INSTANCE_ID];
    uint index = instance.entity;
    vMaterial = instance.material;

    mat4 BoneTransform = mat4(0.0);
    bool withBones = false;
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        int boneIdx = aBoneIds[i];
        if (boneIdx < 0 || boneIdx >= MAX_BONES) {
            continue;
        }
        withBones = true;
        BoneTransform += bones[index][boneIdx] * aWeights[i];
    }
    if (!withBones) {
        BoneTransform = mat4(1.0);
    }

    vec4 newPos = BoneTransform * vec4(aPos, 1.0);
    newPos /= newPos.w;

    mat4 world = model[index];
    gl_Position = world * newPos;
}

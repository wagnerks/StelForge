#version 450 core
#include "sfe.glsl"
#include "sfe_scene.glsl"
// Cascade casters only. cascadeShadowMap.vs stays world-space because the point light pass shares
// it and applies its own cube-face matrices in a geometry shader; this one applies the cascade
// matrix here, which is what let the cascade geometry shader go away.
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aTexCoords;
layout(location = 5) in ivec4 aBoneIds;
layout(location = 6) in vec4 aWeights;
layout(location = 7) in vec2 aTexCoords1;

layout(location = 1) out vec2 TexCoords;
layout(location = 2) out vec2 TexCoords1;
layout(location = 3) flat out uint vMaterial;

layout(SFE_SET(0, 1), std430) readonly buffer modelMatrices {
    mat4 model[];
};

const int MAX_BONE_INFLUENCE = 4;

// Every armature's bones back to back; an instance's start at its boneOffset.
layout(SFE_SET(0, 2), std430) readonly buffer bonesMatrices {
    mat4 bones[];
};

layout(SFE_SET(0, 11), std140) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[6];
};

SFE_PUSH_BLOCK PushConstants {
    int cascadeLayer;
} params;

void main() {
    SfeInstance instance = sfeInstances[SFE_INSTANCE_ID];
    uint index = instance.entity;
    vMaterial = instance.material;

    mat4 BoneTransform = mat4(0.0);
    bool withBones = false;
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        int boneIdx = aBoneIds[i];
        if (boneIdx < 0) {
            continue;
        }
        withBones = true;
        BoneTransform += bones[instance.boneOffset + uint(boneIdx)] * aWeights[i];
    }
    if (!withBones) {
        BoneTransform = mat4(1.0);
    }

    vec4 newPos = BoneTransform * vec4(aPos, 1.0);
    newPos /= newPos.w;

    int layer = clamp(params.cascadeLayer, 0, 5);
    gl_Position = lightSpaceMatrices[layer] * (model[index] * newPos);
    TexCoords = aTexCoords;
    TexCoords1 = aTexCoords1;
}

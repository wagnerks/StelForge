#version 450 core
#include "sfe.glsl"
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

layout(SFE_SET(0, 1), std430) readonly buffer modelMatrices {
    mat4 model[];
};

layout(SFE_SET(0, 3), std430) readonly buffer gpuIndex {
    uint entToGpuIdx[];
};

const int MAX_BONES = 256;
const int MAX_BONE_INFLUENCE = 4;

layout(SFE_SET(0, 2), std430) readonly buffer bonesMatrices {
    mat4 bones[][MAX_BONES];
};

layout(SFE_SET(0, 11), std140) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[6];
};

SFE_PUSH_BLOCK PushConstants {
    float casterOpacity;
    int cascadeLayer;
    float alphaCutoff;
} params;

void main() {
    uint index = entToGpuIdx[SFE_INSTANCE_ID];

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

    int layer = clamp(params.cascadeLayer, 0, 5);
    gl_Position = lightSpaceMatrices[layer] * (model[index] * newPos);
    TexCoords = aTexCoords;
    TexCoords1 = aTexCoords1;
}

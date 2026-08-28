#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangents;
layout(location = 4) in vec3 aBiTangents;
layout(location = 5) in ivec4 aBoneIds;
layout(location = 6) in vec4 aWeights;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec2 TexCoords;
layout(location = 2) out mat3 TBN;

layout(set = 0, binding = 1, std430) readonly buffer modelMatrices {
    mat4 model[];
};

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(set = 0, binding = 2, std430) readonly buffer bonesMatrices {
    mat4 bones[][MAX_BONES];
};

layout(set = 0, binding = 3, std430) readonly buffer gpuIndex {
    uint entToGpuIdx[];
};

layout(set = 0, binding = 5, std140) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

void main() {
    uint index = entToGpuIdx[gl_InstanceIndex];

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

    mat3 normalMatrix = mat3(model[index]) * mat3(BoneTransform);
    vec4 newPos = BoneTransform * vec4(aPos.xyz, 1.0);
    newPos /= newPos.w;
    vec4 worldPos = model[index] * newPos;

    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;

    TBN[0] = normalize(normalMatrix * aTangents);
    TBN[1] = normalize(normalMatrix * aBiTangents);
    TBN[2] = normalize(normalMatrix * aNormal);

    gl_Position = matrices.PV * worldPos;
}

#version 450 core
#include "sfe.glsl"
#include "sfe_scene.glsl"
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangents;
layout(location = 4) in vec3 aBiTangents;
layout(location = 5) in ivec4 aBoneIds;
layout(location = 6) in vec4 aWeights;
layout(location = 7) in vec2 aTexCoords1;

// invariant and `precise` on the skinning chain: otherwise the OpenGL driver and the SPIR-V path may
// fuse multiply-adds differently, and the same skinned vertex lands a few ULPs apart per backend,
// which moves triangle edges by a pixel.
invariant gl_Position;
layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec2 TexCoords;
layout(location = 2) out mat3 TBN;
layout(location = 5) out vec2 TexCoords1;
layout(location = 6) flat out uint vMaterial;

layout(SFE_SET(0, 1), std430) readonly buffer modelMatrices {
    mat4 model[];
};

const int MAX_BONE_INFLUENCE = 4;

// Every armature's bones back to back; an instance's start at its boneOffset.
layout(SFE_SET(0, 2), std430) readonly buffer bonesMatrices {
    mat4 bones[];
};

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

void main() {
    SfeInstance instance = sfeInstances[SFE_INSTANCE_ID];
    uint index = instance.entity;
    vMaterial = instance.material;

    precise mat4 BoneTransform = mat4(0.0);
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

    precise mat3 normalMatrix = mat3(model[index]) * mat3(BoneTransform);
    precise vec4 newPos = BoneTransform * vec4(aPos.xyz, 1.0);
    newPos /= newPos.w;
    precise vec4 worldPos = model[index] * newPos;

    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;
    TexCoords1 = aTexCoords1;

    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalMatrix * aTangents;
    vec3 Bsrc = normalMatrix * aBiTangents;
    T = T - N * dot(N, T);
    if (dot(T, T) > 1e-8) {
        T = normalize(T);
        vec3 B = cross(N, T);
        if (dot(Bsrc, B) < 0.0) {
            B = -B;
        }
        TBN[0] = T;
        TBN[1] = B;
    } else {
        TBN[0] = vec3(0.0);
        TBN[1] = vec3(0.0);
    }
    TBN[2] = N;

    precise vec4 clipPos = matrices.PV * worldPos;
    gl_Position = clipPos;
}

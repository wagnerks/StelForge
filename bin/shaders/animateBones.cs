#version 450 core
#include "sfe.glsl"
layout(local_size_x = 1) in;

const int MAX_BONES = 256;

struct BoneDesc {
    mat4 offset;
    mat4 restLocal;
    int parent;
    int posOff;
    int posCnt;
    int rotOff;
    int rotCnt;
    int sclOff;
    int sclCnt;
    int boneId;
};

struct Vec3Key {
    vec3 value;
    float time;
};

struct QuatKey {
    vec4 q;
    float time;
    float p0;
    float p1;
    float p2;
};

layout(SFE_SET(0, 20), std430) readonly buffer BoneDescs { BoneDesc bones[]; };
layout(SFE_SET(0, 21), std430) readonly buffer PosKeys { Vec3Key posKeys[]; };
layout(SFE_SET(0, 22), std430) readonly buffer RotKeys { QuatKey rotKeys[]; };
layout(SFE_SET(0, 23), std430) readonly buffer SclKeys { Vec3Key sclKeys[]; };
layout(SFE_SET(0, 24), std430) readonly buffer Order { int order[]; };
layout(SFE_SET(0, 2), std430) buffer OutBones { mat4 outBones[]; };

struct AnimInstance {
    mat4 armatureTransform;
    float animTime;
    int entityId;
    int boneCount;
    int boneStride;
};

layout(SFE_SET(0, 25), std430) readonly buffer AnimInstances {
    AnimInstance instances[];
};

// Bone world matrices without the offset, boneStride rows per instance, indexed by bone. A local array of
// them cost NVIDIA's GL driver ~20 ms for a thousand characters.
layout(SFE_SET(0, 26), std430) buffer Worlds { mat4 worlds[]; };

// `precise` along the bone chain: without it each compiler is free to fuse a multiply-add into an fma in
// different places, and the bones differ in the last bits between the OpenGL and Vulkan backends.
mat4 quatToMat(vec4 q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    precise float qxx = x * x;
    precise float qyy = y * y;
    precise float qzz = z * z;
    precise float qxz = x * z;
    precise float qxy = x * y;
    precise float qyz = y * z;
    precise float qwx = w * x;
    precise float qwy = w * y;
    precise float qwz = w * z;

    precise mat4 R = mat4(
        vec4(1.0 - 2.0 * (qyy + qzz), 2.0 * (qxy + qwz),     2.0 * (qxz - qwy),     0.0),
        vec4(2.0 * (qxy - qwz),       1.0 - 2.0 * (qxx + qzz), 2.0 * (qyz + qwx),     0.0),
        vec4(2.0 * (qxz + qwy),       2.0 * (qyz - qwx),     1.0 - 2.0 * (qxx + qyy), 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );
    return R;
}

mat4 localTRS(vec3 t, vec4 q, vec3 s) {
    mat4 R = quatToMat(q);
    mat4 T = mat4(1.0);
    T[3] = vec4(t, 1.0);
    mat4 S = mat4(1.0);
    S[0][0] = s.x;
    S[1][1] = s.y;
    S[2][2] = s.z;
    precise mat4 trs = T * R * S;
    return trs;
}

// The key before the first one later than t, or the second to last when none is: binary searches over
// the first `cnt` keys, which are sorted by time.
int keyIndex(int lo, int cnt) {
    return lo == cnt ? cnt - 2 : lo - 1;
}

int findKey(float t, int off, int cnt) {
    int lo = 1, hi = cnt;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (t < posKeys[off + mid].time) hi = mid; else lo = mid + 1;
    }
    return keyIndex(lo, cnt);
}

int findQuatKey(float t, int off, int cnt) {
    int lo = 1, hi = cnt;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (t < rotKeys[off + mid].time) hi = mid; else lo = mid + 1;
    }
    return keyIndex(lo, cnt);
}

int findSclKey(float t, int off, int cnt) {
    int lo = 1, hi = cnt;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (t < sclKeys[off + mid].time) hi = mid; else lo = mid + 1;
    }
    return keyIndex(lo, cnt);
}

vec3 lerpVec3Keys(int off, int cnt, float t) {
    if (cnt <= 0) {
        return vec3(0.0);
    }
    if (cnt == 1) {
        return posKeys[off].value;
    }
    int i = findKey(t, off, cnt);
    Vec3Key a = posKeys[off + i];
    Vec3Key b = posKeys[off + i + 1];
    precise float f = (t - a.time) / (b.time - a.time);
    precise vec3 v = mix(a.value, b.value, f);
    return v;
}

vec3 lerpSclKeys(int off, int cnt, float t) {
    if (cnt <= 0) {
        return vec3(1.0);
    }
    if (cnt == 1) {
        return sclKeys[off].value;
    }
    int i = findSclKey(t, off, cnt);
    Vec3Key a = sclKeys[off + i];
    Vec3Key b = sclKeys[off + i + 1];
    precise float f = (t - a.time) / (b.time - a.time);
    precise vec3 v = mix(a.value, b.value, f);
    return v;
}

// Normalised lerp, not slerp: acos() and sin() come out in different low bits from the OpenGL driver's
// GLSL compiler and the SPIR-V path, and every bone below inherits that, so skinned vertices landed a
// fraction of a ULP apart between backends. Between two neighbouring animation keys the angle is small
// and nlerp stays within a hair of slerp.
vec4 nlerpQuat(vec4 q0, vec4 q1, float inFraction) {
    float sign1 = dot(q0, q1) < 0.0 ? -1.0 : 1.0;
    precise vec4 q = normalize((1.0 - inFraction) * q0 + (sign1 * inFraction) * q1);
    return q;
}

vec4 lerpQuatKeys(int off, int cnt, float t) {
    if (cnt <= 0) {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }
    if (cnt == 1) {
        return normalize(rotKeys[off].q);
    }
    int i = findQuatKey(t, off, cnt);
    QuatKey a = rotKeys[off + i];
    QuatKey b = rotKeys[off + i + 1];
    precise float f = (t - a.time) / (b.time - a.time);
    return nlerpQuat(a.q, b.q, f);
}

void main() {
    AnimInstance inst = instances[gl_WorkGroupID.x];
    int row = int(gl_WorkGroupID.x) * inst.boneStride;
    for (int i = 0; i < inst.boneCount; ++i) {
        int bi = order[i];
        BoneDesc b = bones[bi];

        precise mat4 local = b.restLocal;
        if (b.posCnt > 0 || b.rotCnt > 0 || b.sclCnt > 0) {
            vec3 pos = b.restLocal[3].xyz;
            vec3 scl = vec3(length(b.restLocal[0].xyz), length(b.restLocal[1].xyz), length(b.restLocal[2].xyz));
            if (b.posCnt > 0) {
                pos = lerpVec3Keys(b.posOff, b.posCnt, inst.animTime);
            }
            if (b.sclCnt > 0) {
                scl = lerpSclKeys(b.sclOff, b.sclCnt, inst.animTime);
            }
            if (b.rotCnt > 0) {
                vec4 rot = lerpQuatKeys(b.rotOff, b.rotCnt, inst.animTime);
                if (scl.x < 1e-8) scl.x = 1.0;
                if (scl.y < 1e-8) scl.y = 1.0;
                if (scl.z < 1e-8) scl.z = 1.0;
                local = localTRS(pos, rot, scl);
            } else {
                local[3] = vec4(pos, 1.0);
            }
        }

        mat4 parent = (b.parent < 0) ? inst.armatureTransform : worlds[row + b.parent];
        precise mat4 world = parent * local;
        worlds[row + bi] = world;
        precise mat4 skin = world * b.offset;
        outBones[inst.entityId * MAX_BONES + b.boneId] = skin;
    }
}

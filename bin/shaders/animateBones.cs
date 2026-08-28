#version 430
layout(local_size_x = 1) in;

const int MAX_BONES = 100;

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

layout(std430, binding = 20) readonly buffer BoneDescs { BoneDesc bones[]; };
layout(std430, binding = 21) readonly buffer PosKeys { Vec3Key posKeys[]; };
layout(std430, binding = 22) readonly buffer RotKeys { QuatKey rotKeys[]; };
layout(std430, binding = 23) readonly buffer SclKeys { Vec3Key sclKeys[]; };
layout(std430, binding = 24) readonly buffer Order { int order[]; };
layout(std430, binding = 2) buffer OutBones { mat4 outBones[]; };

struct AnimInstance {
    mat4 armatureTransform;
    float animTime;
    int entityId;
    int boneCount;
};

layout(std430, binding = 25) readonly buffer AnimInstances {
    AnimInstance instances[];
};

mat4 quatToMat(vec4 q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float qxx = x * x;
    float qyy = y * y;
    float qzz = z * z;
    float qxz = x * z;
    float qxy = x * y;
    float qyz = y * z;
    float qwx = w * x;
    float qwy = w * y;
    float qwz = w * z;

    return mat4(
        vec4(1.0 - 2.0 * (qyy + qzz), 2.0 * (qxy + qwz),     2.0 * (qxz - qwy),     0.0),
        vec4(2.0 * (qxy - qwz),       1.0 - 2.0 * (qxx + qzz), 2.0 * (qyz + qwx),     0.0),
        vec4(2.0 * (qxz + qwy),       2.0 * (qyz - qwx),     1.0 - 2.0 * (qxx + qyy), 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );
}

mat4 localTRS(vec3 t, vec4 q, vec3 s) {
    mat4 R = quatToMat(q);
    mat4 T = mat4(1.0);
    T[3] = vec4(t, 1.0);
    mat4 S = mat4(1.0);
    S[0][0] = s.x;
    S[1][1] = s.y;
    S[2][2] = s.z;
    return T * R * S;
}

int findKey(float t, int off, int cnt) {
    int index = 1;
    for (; index < cnt; ++index) {
        if (t < posKeys[off + index].time) {
            return index - 1;
        }
    }
    return cnt - 2;
}

int findQuatKey(float t, int off, int cnt) {
    int index = 1;
    for (; index < cnt; ++index) {
        if (t < rotKeys[off + index].time) {
            return index - 1;
        }
    }
    return cnt - 2;
}

int findSclKey(float t, int off, int cnt) {
    int index = 1;
    for (; index < cnt; ++index) {
        if (t < sclKeys[off + index].time) {
            return index - 1;
        }
    }
    return cnt - 2;
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
    float f = (t - a.time) / (b.time - a.time);
    return mix(a.value, b.value, f);
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
    float f = (t - a.time) / (b.time - a.time);
    return mix(a.value, b.value, f);
}

vec4 slerpQuat(vec4 q0, vec4 q1, float inFraction) {
    float signScale1 = 1.0;
    float cosOmega = clamp(dot(q0, q1), -1.0, 1.0);
    if (cosOmega < 0.0) {
        cosOmega = -cosOmega;
        signScale1 = -1.0;
    }

    float scale0;
    float scale1;
    if (1.0 - cosOmega > 0.0001) {
        float omega = acos(cosOmega);
        float sinOmega = sin(omega);
        scale0 = sin((1.0 - inFraction) * omega) / sinOmega;
        scale1 = signScale1 * sin(inFraction * omega) / sinOmega;
    } else {
        scale0 = 1.0 - inFraction;
        scale1 = signScale1 * inFraction;
    }
    return normalize(scale0 * q0 + scale1 * q1);
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
    float f = (t - a.time) / (b.time - a.time);
    return slerpQuat(a.q, b.q, f);
}

void main() {
    AnimInstance inst = instances[gl_WorkGroupID.x];
    mat4 worlds[MAX_BONES];
    for (int i = 0; i < inst.boneCount; ++i) {
        int bi = order[i];
        BoneDesc b = bones[bi];

        mat4 local = b.restLocal;
        if (b.posCnt > 0 || b.rotCnt > 0 || b.sclCnt > 0) {
            vec3 pos = lerpVec3Keys(b.posOff, b.posCnt, inst.animTime);
            vec4 rot = lerpQuatKeys(b.rotOff, b.rotCnt, inst.animTime);
            vec3 scl = lerpSclKeys(b.sclOff, b.sclCnt, inst.animTime);
            local = localTRS(pos, rot, scl);
        }

        mat4 parent = (b.parent < 0) ? inst.armatureTransform : worlds[b.parent];
        mat4 world = parent * local;
        worlds[b.boneId] = world;
        outBones[inst.entityId * MAX_BONES + b.boneId] = world * b.offset;
    }
}

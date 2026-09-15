#version 450 core
#include "sfe.glsl"
// One invocation per animated character, all clips at once.
layout(local_size_x = 16) in;


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
    float radius;    // the farthest vertex weighted to the bone, in its bind space
    uint boundsMask; // the pose boxes of the meshes it moves
    float _p0;
    float _p1;
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

const uint kPlaying = 1u;
const uint kLoop = 2u;

// Written only when the character's animation or armature changes.
struct AnimRecord {
    mat4 armatureTransform;
    uint boneOffset; // the entity's first matrix in OutBones, and its first row in Worlds
    uint descOffset; // the clip's first entry in BoneDescs and Order
    uint boneCount;
    uint flags;      // kPlaying, kLoop, and the pose bounds count in bits 8-15
    float start;     // seconds on the clock at which the clip was at tick 0
    float ticksPerSecond;
    float duration;
    float stoppedTime; // the tick shown while not playing
};

layout(SFE_SET(0, 25), std430) readonly buffer AnimRecords {
    AnimRecord records[];
};

SFE_PUSH_BLOCK AnimClock {
    float now; // seconds, on the same epoch as the records' start
    uint count;
} clock;

// Bone world matrices without the offset, indexed like OutBones. A local array of them cost NVIDIA's GL
// driver ~20 ms for a thousand characters.
layout(SFE_SET(0, 26), std430) buffer Worlds { mat4 worlds[]; };

// `precise` along the bone chain: without it each compiler is free to fuse a multiply-add into an fma in
// different places, and the bones differ in the last bits between the OpenGL and Vulkan backends. For the same
// reason mix(), normalize() and length() are written out below: the two paths implement those built-ins with
// different formulas or sum orders, which `precise` cannot pin.
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
    precise vec3 v = a.value * (1.0 - f) + b.value * f;
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
    precise vec3 v = a.value * (1.0 - f) + b.value * f;
    return v;
}

// Normalised lerp, not slerp: acos() and sin() come out in different low bits from the OpenGL driver's
// GLSL compiler and the SPIR-V path, and every bone below inherits that, so skinned vertices landed a
// fraction of a ULP apart between backends. Between two neighbouring animation keys the angle is small
// and nlerp stays within a hair of slerp.
vec4 nlerpQuat(vec4 q0, vec4 q1, float inFraction) {
    float sign1 = dot(q0, q1) < 0.0 ? -1.0 : 1.0;
    precise vec4 qs = (1.0 - inFraction) * q0 + (sign1 * inFraction) * q1;
    precise vec4 q = qs / sqrt(((qs.x * qs.x + qs.y * qs.y) + qs.z * qs.z) + qs.w * qs.w);
    return q;
}

vec4 lerpQuatKeys(int off, int cnt, float t) {
    if (cnt <= 0) {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }
    if (cnt == 1) {
        precise vec4 q = rotKeys[off].q;
        precise vec4 qn = q / sqrt(((q.x * q.x + q.y * q.y) + q.z * q.z) + q.w * q.w);
        return qn;
    }
    int i = findQuatKey(t, off, cnt);
    QuatKey a = rotKeys[off + i];
    QuatKey b = rotKeys[off + i + 1];
    precise float f = (t - a.time) / (b.time - a.time);
    return nlerpQuat(a.q, b.q, f);
}

// AnimationComponent::time() on the CPU is the same formula.
float animTime(AnimRecord r) {
    if ((r.flags & kPlaying) == 0u) {
        return r.stoppedTime;
    }
    precise float t = (clock.now - r.start) * r.ticksPerSecond;
    if (r.duration <= 0.0) {
        return t;
    }
    if ((r.flags & kLoop) != 0u) {
        precise float looped = t - r.duration * floor(t / r.duration);
        return looped;
    }
    return min(t, r.duration);
}

void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= clock.count) {
        return;
    }
    AnimRecord inst = records[index];
    float time = animTime(inst);
    int row = int(inst.boneOffset);
    int desc = int(inst.descOffset);
    // The pose bounds of the entity's skinned meshes, in the matrices before its bones, grown below from every posed
    // joint by its radius: column 0 the minimum, column 1 the maximum, in the space the skinned vertices land in.
    uint boundsCount = (inst.flags >> 8) & 0xFFu;
    for (uint j = 0u; j < boundsCount; ++j) {
        outBones[row - 1 - int(j)] = mat4(vec4(vec3(3.0e38), 1.0), vec4(vec3(-3.0e38), 1.0), vec4(0.0), vec4(0.0));
    }
    for (int i = 0; i < int(inst.boneCount); ++i) {
        int bi = order[desc + i];
        BoneDesc b = bones[desc + bi];

        precise mat4 local = b.restLocal;
        if (b.posCnt > 0 || b.rotCnt > 0 || b.sclCnt > 0) {
            vec3 pos = b.restLocal[3].xyz;
            precise vec3 scl = sqrt(vec3(dot(b.restLocal[0].xyz, b.restLocal[0].xyz), dot(b.restLocal[1].xyz, b.restLocal[1].xyz), dot(b.restLocal[2].xyz, b.restLocal[2].xyz)));
            if (b.posCnt > 0) {
                pos = lerpVec3Keys(b.posOff, b.posCnt, time);
            }
            if (b.sclCnt > 0) {
                scl = lerpSclKeys(b.sclOff, b.sclCnt, time);
            }
            if (b.rotCnt > 0) {
                vec4 rot = lerpQuatKeys(b.rotOff, b.rotCnt, time);
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
        outBones[row + b.boneId] = skin;

        if (b.boundsMask != 0u) {
            // Read back instead of reusing `world`: a second consumer of it made the OpenGL compiler evaluate the
            // bone chain differently, and skinned vertices moved a few ULPs on that backend alone.
            mat4 posed = worlds[row + bi];
            // Sums written out, like the rest of this file: dot() is summed in a different order on each backend.
            float sx = (posed[0].x * posed[0].x + posed[0].y * posed[0].y) + posed[0].z * posed[0].z;
            float sy = (posed[1].x * posed[1].x + posed[1].y * posed[1].y) + posed[1].z * posed[1].z;
            float sz = (posed[2].x * posed[2].x + posed[2].y * posed[2].y) + posed[2].z * posed[2].z;
            float scale = sqrt(max(sx, max(sy, sz)));
            vec3 reach = vec3(b.radius * scale);
            vec3 lo = posed[3].xyz - reach;
            vec3 hi = posed[3].xyz + reach;
            for (uint j = 0u; j < boundsCount; ++j) {
                if ((b.boundsMask & (1u << j)) != 0u) {
                    int slot = row - 1 - int(j);
                    mat4 box = outBones[slot];
                    box[0].xyz = min(box[0].xyz, lo);
                    box[1].xyz = max(box[1].xyz, hi);
                    outBones[slot] = box;
                }
            }
        }
    }
}

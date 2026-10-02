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
    // Ticks between a channel's keys when every gap is the same, else 0.
    float posStep;
    float rotStep;
    float sclStep;
    float _p0;
    float _p1;
    float _p2;
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
// Last frame's pose, for the motion vectors the G-buffer writes: each bone's matrix is moved here before the
// new one overwrites it.
layout(SFE_SET(0, 9), std430) buffer PrevBones { mat4 prevBones[]; };

const uint kPlaying = 1u;
const uint kLoop = 2u;
const uint kBlending = 4u;

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
    // The clip being faded out of, over the same armature; fromFlags has kBlending while there is one.
    uint fromDescOffset;
    uint fromFlags;
    float fromStart;
    float fromTicksPerSecond;
    float fromDuration;
    float fromStoppedTime;
    float blendStart;
    float blendDuration;
    // A turn laid over the clip in the model's space (AnimationComponent::mLook), made in part by each of up to four
    // bones about its own joint; -1 for no bone.
    vec4 look;
    ivec4 lookBone;
    vec4 lookShare;
    // And AnimationComponent::mTwist the same way, laid on before the look.
    vec4 twist;
    ivec4 twistBone;
    vec4 twistShare;
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

// With evenly spaced keys (step > 0) the key is worked out directly: a search's loop of buffer reads cost the pass
// more than all its matrix work. The search is kept for clips whose keys are not evenly spaced.
int keyForStep(float t, float firstTime, float step, int cnt) {
    return clamp(int(floor((t - firstTime) / step)), 0, cnt - 2);
}

int findKey(float t, int off, int cnt, float step) {
    if (step > 0.0) {
        return keyForStep(t, posKeys[off].time, step, cnt);
    }
    int lo = 1, hi = cnt;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (t < posKeys[off + mid].time) hi = mid; else lo = mid + 1;
    }
    return keyIndex(lo, cnt);
}

int findQuatKey(float t, int off, int cnt, float step) {
    if (step > 0.0) {
        return keyForStep(t, rotKeys[off].time, step, cnt);
    }
    int lo = 1, hi = cnt;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (t < rotKeys[off + mid].time) hi = mid; else lo = mid + 1;
    }
    return keyIndex(lo, cnt);
}

int findSclKey(float t, int off, int cnt, float step) {
    if (step > 0.0) {
        return keyForStep(t, sclKeys[off].time, step, cnt);
    }
    int lo = 1, hi = cnt;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (t < sclKeys[off + mid].time) hi = mid; else lo = mid + 1;
    }
    return keyIndex(lo, cnt);
}

vec3 lerpVec3Keys(int off, int cnt, float t, float step) {
    if (cnt <= 0) {
        return vec3(0.0);
    }
    if (cnt == 1) {
        return posKeys[off].value;
    }
    int i = findKey(t, off, cnt, step);
    Vec3Key a = posKeys[off + i];
    Vec3Key b = posKeys[off + i + 1];
    precise float f = (t - a.time) / (b.time - a.time);
    precise vec3 v = a.value * (1.0 - f) + b.value * f;
    return v;
}

vec3 lerpSclKeys(int off, int cnt, float t, float step) {
    if (cnt <= 0) {
        return vec3(1.0);
    }
    if (cnt == 1) {
        return sclKeys[off].value;
    }
    int i = findSclKey(t, off, cnt, step);
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

// A bone's world turned by its share of `turn` (the shares of up to four bones, -1 for none): about the joint, the
// rotation turns, the joint stays where the clip put it.
mat4 turnedBy(mat4 world, int bi, vec4 turn, ivec4 bones, vec4 shares) {
    float share = 0.0;
    for (int k = 0; k < 4; ++k) {
        share += bi == bones[k] ? shares[k] : 0.0;
    }
    if (share <= 0.0) {
        return world;
    }
    mat4 by = quatToMat(nlerpQuat(vec4(0.0, 0.0, 0.0, 1.0), turn, share));
    return mat4(by * world[0], by * world[1], by * world[2], world[3]);
}

vec4 lerpQuatKeys(int off, int cnt, float t, float step) {
    if (cnt <= 0) {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }
    if (cnt == 1) {
        precise vec4 q = rotKeys[off].q;
        precise vec4 qn = q / sqrt(((q.x * q.x + q.y * q.y) + q.z * q.z) + q.w * q.w);
        return qn;
    }
    int i = findQuatKey(t, off, cnt, step);
    QuatKey a = rotKeys[off + i];
    QuatKey b = rotKeys[off + i + 1];
    precise float f = (t - a.time) / (b.time - a.time);
    return nlerpQuat(a.q, b.q, f);
}

// AnimationComponent::time() on the CPU is the same formula.
float clipTime(uint flags, float start, float ticksPerSecond, float duration, float stoppedTime) {
    if ((flags & kPlaying) == 0u) {
        return stoppedTime;
    }
    precise float t = (clock.now - start) * ticksPerSecond;
    if (duration <= 0.0) {
        return t;
    }
    if ((flags & kLoop) != 0u) {
        precise float looped = t - duration * floor(t / duration);
        return looped;
    }
    return min(t, duration);
}

float animTime(AnimRecord r) {
    return clipTime(r.flags, r.start, r.ticksPerSecond, r.duration, r.stoppedTime);
}

// The rotation part of a matrix without scale or shear, as a quaternion.
vec4 matToQuat(mat3 m) {
    float trace = m[0][0] + m[1][1] + m[2][2];
    vec4 q;
    if (trace > 0.0) {
        float s = 0.5 / sqrt(trace + 1.0);
        q = vec4((m[1][2] - m[2][1]) * s, (m[2][0] - m[0][2]) * s, (m[0][1] - m[1][0]) * s, 0.25 / s);
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        float s = 2.0 * sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]);
        q = vec4(0.25 * s, (m[0][1] + m[1][0]) / s, (m[2][0] + m[0][2]) / s, (m[1][2] - m[2][1]) / s);
    } else if (m[1][1] > m[2][2]) {
        float s = 2.0 * sqrt(1.0 - m[0][0] + m[1][1] - m[2][2]);
        q = vec4((m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s, (m[2][0] - m[0][2]) / s);
    } else {
        float s = 2.0 * sqrt(1.0 - m[0][0] - m[1][1] + m[2][2]);
        q = vec4((m[2][0] + m[0][2]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s, (m[0][1] - m[1][0]) / s);
    }
    return normalize(q);
}

// A bone's local position, rotation and scale at `time`: its keys where it has them, its rest pose elsewhere.
void samplePose(BoneDesc b, float time, out vec3 pos, out vec4 rot, out vec3 scl) {
    mat4 rest = b.restLocal;
    scl = vec3(length(rest[0].xyz), length(rest[1].xyz), length(rest[2].xyz));
    pos = b.posCnt > 0 ? lerpVec3Keys(b.posOff, b.posCnt, time, b.posStep) : rest[3].xyz;
    rot = b.rotCnt > 0 ? lerpQuatKeys(b.rotOff, b.rotCnt, time, b.rotStep)
        : matToQuat(mat3(rest[0].xyz / max(scl.x, 1e-8), rest[1].xyz / max(scl.y, 1e-8), rest[2].xyz / max(scl.z, 1e-8)));
    if (b.sclCnt > 0) {
        scl = lerpSclKeys(b.sclOff, b.sclCnt, time, b.sclStep);
    }
    if (scl.x < 1e-8) scl.x = 1.0;
    if (scl.y < 1e-8) scl.y = 1.0;
    if (scl.z < 1e-8) scl.z = 1.0;
}

void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= clock.count) {
        return;
    }
    AnimRecord inst = records[index];
    float time = animTime(inst);
    // How far into the current clip a crossfade has come; 1 when there is none, and then the blend is skipped.
    float blend = 1.0;
    float fromTime = 0.0;
    if ((inst.fromFlags & kBlending) != 0u && inst.blendDuration > 0.0) {
        blend = clamp((clock.now - inst.blendStart) / inst.blendDuration, 0.0, 1.0);
        fromTime = clipTime(inst.fromFlags, inst.fromStart, inst.fromTicksPerSecond, inst.fromDuration, inst.fromStoppedTime);
    }
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
        if (blend < 1.0) {
            vec3 pos, scl, fromPos, fromScl;
            vec4 rot, fromRot;
            samplePose(b, time, pos, rot, scl);
            samplePose(bones[inst.fromDescOffset + bi], fromTime, fromPos, fromRot, fromScl);
            local = localTRS(mix(fromPos, pos, blend), nlerpQuat(fromRot, rot, blend), mix(fromScl, scl, blend));
        }
        else if (b.posCnt > 0 || b.rotCnt > 0 || b.sclCnt > 0) {
            vec3 pos = b.restLocal[3].xyz;
            precise vec3 scl = sqrt(vec3(dot(b.restLocal[0].xyz, b.restLocal[0].xyz), dot(b.restLocal[1].xyz, b.restLocal[1].xyz), dot(b.restLocal[2].xyz, b.restLocal[2].xyz)));
            if (b.posCnt > 0) {
                pos = lerpVec3Keys(b.posOff, b.posCnt, time, b.posStep);
            }
            if (b.sclCnt > 0) {
                scl = lerpSclKeys(b.sclOff, b.sclCnt, time, b.sclStep);
            }
            if (b.rotCnt > 0) {
                vec4 rot = lerpQuatKeys(b.rotOff, b.rotCnt, time, b.rotStep);
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
        world = turnedBy(world, bi, inst.twist, inst.twistBone, inst.twistShare);
        world = turnedBy(world, bi, inst.look, inst.lookBone, inst.lookShare);
        worlds[row + bi] = world;
        precise mat4 skin = world * b.offset;
        prevBones[row + b.boneId] = outBones[row + b.boneId];
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

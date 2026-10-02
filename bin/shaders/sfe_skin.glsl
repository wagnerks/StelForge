#ifndef SFE_SKIN_GLSL
#define SFE_SKIN_GLSL
// Skinning by dual quaternions (Kavan et al.): each bone's matrix taken apart into a turn and a move, those blended as
// dual quaternions and put back together -- so a joint bent far keeps its volume, where a plain weighted sum of the
// matrices (linear blend) pinches it flat. Whatever scale or shear a bone's matrix carries on top of its turn
// (S = R^T M) is blended as a plain sum and applied first, before the turn and the move.
//
// That scale is about the model's origin, not the bone's joint, and each bone's move makes up for it only under its
// own turn: blended with a neighbour's turn, a bone stretched along its length throws the skin they share far off --
// a forearm stretched to twice its length pulls the wrist past the hand. So a vertex hung from a bone stretched
// unevenly is blended linearly instead, which puts it between where each bone alone would, and the dual
// quaternions are kept for every other.
//
//     SfeSkin skin = sfeSkinNone();
//     sfeSkinAdd(skin, bones[...], weight);   // for each bone the vertex hangs from
//     mat4 BoneTransform = sfeSkinMatrix(skin);   // the identity when it hangs from none

struct SfeSkin {
    vec4 real;     // the turn: xyz the vector part, w the scalar
    vec4 dual;     // the move
    vec4 pivot;    // the first bone's turn: every other one taken on its side of it, so none cancels another out
    mat3 scale;
    float weight;
    mat4 linear;   // the plain weighted sum of the matrices
    float uneven;  // how unevenly its bones are scaled, weighted
};

// How uneven a bone's scale may get before its vertices are blended linearly outright: a twentieth off even.
const float kSfeSkinLinearFrom = 20.0;

SfeSkin sfeSkinNone() {
    return SfeSkin(vec4(0.0), vec4(0.0), vec4(0.0), mat3(0.0), 0.0, mat4(0.0), 0.0);
}

vec4 sfeQuatMul(vec4 a, vec4 b) {
    return vec4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz), a.w * b.w - dot(a.xyz, b.xyz));
}

// A turn's quaternion from its matrix (columns), the largest of its four parts found first so it never divides by
// nearly nothing.
vec4 sfeQuatOf(mat3 m) {
    float t = m[0][0] + m[1][1] + m[2][2];
    if (t > 0.0) {
        float s = sqrt(t + 1.0) * 2.0;
        return vec4((m[1][2] - m[2][1]) / s, (m[2][0] - m[0][2]) / s, (m[0][1] - m[1][0]) / s, 0.25 * s);
    }
    if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        float s = sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
        return vec4(0.25 * s, (m[1][0] + m[0][1]) / s, (m[2][0] + m[0][2]) / s, (m[1][2] - m[2][1]) / s);
    }
    if (m[1][1] > m[2][2]) {
        float s = sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
        return vec4((m[1][0] + m[0][1]) / s, 0.25 * s, (m[2][1] + m[1][2]) / s, (m[2][0] - m[0][2]) / s);
    }
    float s = sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
    return vec4((m[2][0] + m[0][2]) / s, (m[2][1] + m[1][2]) / s, 0.25 * s, (m[0][1] - m[1][0]) / s);
}

mat3 sfeMat3Of(vec4 q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    return mat3(1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + w * z), 2.0 * (x * z - w * y),
                2.0 * (x * y - w * z), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + w * x),
                2.0 * (x * z + w * y), 2.0 * (y * z - w * x), 1.0 - 2.0 * (x * x + y * y));
}

void sfeSkinAdd(inout SfeSkin skin, mat4 bone, float weight) {
    if (weight <= 0.0) {
        return;
    }
    // Its turn squared up out of whatever scale it carries; a mirrored one keeps the mirror in its scale.
    mat3 m = mat3(bone);
    vec3 x = normalize(m[0]);
    vec3 y = normalize(m[1] - x * dot(x, m[1]));
    mat3 turn = mat3(x, y, cross(x, y));
    vec4 real = sfeQuatOf(turn);
    if (skin.weight <= 0.0) {
        skin.pivot = real;
    }
    else if (dot(real, skin.pivot) < 0.0) {
        real = -real;
    }
    vec4 dual = 0.5 * sfeQuatMul(vec4(bone[3].xyz, 0.0), real);
    mat3 scale = transpose(turn) * m;
    // What is left once an even scale is taken out, against that scale: 0 for a bone scaled alike every way.
    float even = (scale[0][0] + scale[1][1] + scale[2][2]) / 3.0;
    mat3 off = scale - mat3(even);
    skin.uneven += sqrt(dot(off[0], off[0]) + dot(off[1], off[1]) + dot(off[2], off[2])) / max(abs(even), 1e-6) * weight;
    skin.real += real * weight;
    skin.dual += dual * weight;
    skin.scale += scale * weight;
    skin.linear += bone * weight;
    skin.weight += weight;
}

mat4 sfeSkinMatrix(SfeSkin skin) {
    float size = length(skin.real);
    if (skin.weight <= 0.0 || size < 1e-6) {
        return mat4(1.0);
    }
    vec4 real = skin.real / size;
    vec4 dual = skin.dual / size;
    vec3 move = 2.0 * sfeQuatMul(dual, vec4(-real.xyz, real.w)).xyz;
    mat4 blended = mat4(sfeMat3Of(real) * (skin.scale / skin.weight));
    blended[3] = vec4(move, 1.0);
    // Eased over from one to the other as a bone starts to stretch, so nothing jumps when it does.
    float linear = clamp(skin.uneven / skin.weight * kSfeSkinLinearFrom, 0.0, 1.0);
    return linear > 0.0 ? blended + (skin.linear / skin.weight - blended) * linear : blended;
}
#endif

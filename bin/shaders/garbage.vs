#version 450 core
#include "sfe.glsl"
// A piece of GarbagePass's litter: GarbageSystem's record of it, picked by the frame's visible list, turns and places
// the kind's mesh. Into the G-buffer it hands the default surface shader what g_buffer.vs would; into a shadow map
// only its position.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangents;
layout(location = 4) in vec3 aBiTangents;
layout(location = 7) in vec2 aTexCoords1;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec2 TexCoords;
layout(location = 2) out mat3 TBN;
layout(location = 5) out vec2 TexCoords1;
layout(location = 6) flat out uint vMaterial;
layout(location = 7) out vec4 vClip;
layout(location = 8) out vec4 vPrevClip;
layout(location = 9) out vec3 vSurface; // see g_buffer.vs

// Mirrors GarbageSystem::GpuPiece.
struct SfeGarbagePiece {
    vec4 posScale; // xyz where its origin is, w its scale
    vec4 rotation; // quaternion xyzw
};
layout(SFE_SET(0, 20), std430) readonly buffer GarbagePieces {
    SfeGarbagePiece garbagePieces[];
};
layout(SFE_SET(0, 21), std430) readonly buffer GarbageVisible {
    uint garbageVisible[];
};
layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

// Mirrors GarbagePass::DrawParams.
SFE_PUSH_BLOCK GarbageDraw {
    mat4 viewProj; // a light's, into a shadow map
    vec4 params;   // x 1 into a light's view, y the material
} draw;

vec3 turn(vec4 q, vec3 v) {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

void main() {
    SfeGarbagePiece piece = garbagePieces[garbageVisible[SFE_INSTANCE_ID]];
    vec4 world = vec4(piece.posScale.xyz + turn(piece.rotation, aPos * piece.posScale.w), 1.0);

    FragPos = world.xyz;
    vSurface = world.xyz - piece.posScale.xyz;
    TexCoords = aTexCoords;
    TexCoords1 = aTexCoords1;
    vMaterial = uint(draw.params.y + 0.5);
    if (draw.params.x > 0.5) {
        TBN = mat3(1.0);
        vClip = vec4(0.0);
        vPrevClip = vec4(0.0);
        gl_Position = draw.viewProj * world;
        gl_Position.z = max(gl_Position.z, -gl_Position.w); // pancaked; see cascadeShadowMap_csm.vs
        return;
    }

    vec3 N = normalize(turn(piece.rotation, aNormal));
    vec3 T = turn(piece.rotation, aTangents);
    vec3 Bsrc = turn(piece.rotation, aBiTangents);
    T = T - N * dot(N, T);
    if (dot(T, T) > 1e-8) {
        T = normalize(T);
        vec3 B = cross(N, T);
        if (dot(Bsrc, B) < 0.0) {
            B = -B;
        }
        TBN = mat3(T, B, N);
    } else {
        TBN = mat3(vec3(0.0), vec3(0.0), N);
    }

    // It does not move while it is drawn from here: last frame it was where it is.
    vClip = matrices.PVNoJitter * world;
    vPrevClip = matrices.prevPVNoJitter * world;
    gl_Position = matrices.PV * world;
}

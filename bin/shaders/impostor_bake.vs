#version 450 core
#include "sfe.glsl"
#define SFE_IMPOSTOR_LAYOUT_ONLY
#include "sfe_impostor.glsl"
// One submesh of a model, seen along the horizontal direction `view.xy` from far away, into its frame of the atlas.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUv;
layout(location = 2) out float vOcclusion;

// Mirrors ImpostorPass::BakeParams.
SFE_PUSH_BLOCK ImpostorBake {
    vec4 view;  // xy sin and cos of the direction the camera sits in, z half the frame's side, w its centre's height
    vec4 frame; // atlas UV rect
    vec4 tint;  // rgb multiplies the albedo, a the alpha cutoff
} bake;

void main() {
    vec3 towards = vec3(bake.view.x, 0.0, bake.view.y);
    vec3 right = vec3(bake.view.y, 0.0, -bake.view.x);
    vec2 local = vec2(dot(aPos, right), aPos.y - bake.view.w) / bake.view.z;
    float depth = dot(aPos, towards) / bake.view.z;
    vec2 frameUv = kSfeImpostorMargin + (local * 0.5 + 0.5) * (1.0 - 2.0 * kSfeImpostorMargin);
    vec2 atlasUv = bake.frame.xy + frameUv * bake.frame.zw;
    gl_Position = vec4(atlasUv * 2.0 - 1.0, -depth, 1.0);
    vNormal = aNormal;
    // How buried in the crown this leaf is, since a picture has no shadows of its own: the far side of the crown, its
    // middle and its lower part are in the shade of the rest, which the real tree gets from its shadow map and SSAO.
    float front = clamp(depth * 0.5 + 0.5, 0.0, 1.0);
    float outer = smoothstep(0.0, 0.7, length(aPos.xz) / bake.view.z);
    float up = smoothstep(-0.8, 0.8, (aPos.y - bake.view.w) / bake.view.z);
    vOcclusion = mix(0.45, 1.0, front) * mix(0.75, 1.0, outer) * mix(0.8, 1.0, up);
    vUv = aTexCoords;
}

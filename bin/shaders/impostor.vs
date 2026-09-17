#version 450 core
#include "sfe.glsl"
#define SFE_WIND_TREES_ONLY
#include "sfe_wind.glsl"
#include "sfe_impostor.glsl"
// A far tree as a picture of itself: a quad standing upright where the tree does, turned to the camera (or, into a
// shadow map, to the light), showing the two baked views nearest the direction it is seen from.
layout(location = 0) in vec2 aPos; // corner, -1..1

layout(location = 0) out vec2 vUv; // inside the frame, 0..1
layout(location = 1) flat out vec4 vFrame0;
layout(location = 2) flat out vec4 vFrame1;
layout(location = 3) flat out vec3 vBlendYaw; // x the share of the second view, yz cos and sin of the yaw
layout(location = 4) out vec3 vWorld;
layout(location = 5) out vec4 vClip;
layout(location = 6) out vec4 vPrevClip;

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

void collapse() {
    gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
    vUv = vec2(0.0);
    vFrame0 = vec4(0.0);
    vFrame1 = vec4(0.0);
    vBlendYaw = vec3(0.0);
    vWorld = vec3(0.0);
    vClip = vec4(0.0);
    vPrevClip = vec4(0.0);
}

void main() {
    SfeImpostorInstance instance = impostors[SFE_INSTANCE_ID];
    uint placement = uint(instance.yawModel.z + 0.5);
    if ((entityBits[placement >> 5u] & (1u << (placement & 31u))) != 0u) {
        collapse();
        return;
    }
    int model = int(instance.yawModel.y + 0.5);
    vec4 frame = impostorModels[model];
    float scale = instance.posScale.w;
    float halfSize = frame.x * scale;
    vec3 centre = instance.posScale.xyz + vec3(0.0, frame.y * scale, 0.0);

    bool light = draw.eye.w > 0.5;
    vec2 toEye = light ? -draw.eye.xz : draw.eye.xz - centre.xz;
    float toEyeLength = length(toEye);
    toEye = toEyeLength > 1e-4 ? toEye / toEyeLength : vec2(0.0, 1.0);

    // The direction it is seen from, in the model's own space, picks the views.
    float c = cos(instance.yawModel.x);
    float s = sin(instance.yawModel.x);
    vec2 local = vec2(toEye.x * c - toEye.y * s, toEye.x * s + toEye.y * c);
    float views = fract(atan(local.x, local.y) / 6.2831853) * float(kSfeImpostorViews);
    int view0 = int(floor(views)) % kSfeImpostorViews;
    int view1 = (view0 + 1) % kSfeImpostorViews;

    float distanceToEye = light ? 1.0 : max(distance(draw.eye.xyz, centre), 1.0);
    float texels = 2.0 * halfSize * draw.params.y / distanceToEye;
    int level = clamp(int(floor(log2(max(draw.atlas.x / max(texels, 1.0), 1.0)))), 0, kSfeImpostorLevels - 1);

    vec3 right = vec3(toEye.y, 0.0, -toEye.x);
    vec3 world = centre + right * aPos.x * halfSize + vec3(0.0, aPos.y * halfSize, 0.0);
    // The picture leans in the wind as the tree it stands for does: its foot stays put and its crown bends.
    vec3 still = world;
    world += sfeWindSwayTree(still, instance.posScale.xyz, matrices.wind);
    if (light) {
        // Behind its own tree as the light sees it, so the picture does not shadow the half of itself in front.
        world += draw.eye.xyz * halfSize * draw.params.x;
    }

    vUv = kSfeImpostorMargin + (aPos * 0.5 + 0.5) * (1.0 - 2.0 * kSfeImpostorMargin);
    vFrame0 = sfeImpostorFrame(model, view0, level);
    vFrame1 = sfeImpostorFrame(model, view1, level);
    vBlendYaw = vec3(fract(views), c, s);
    vWorld = world;
    if (light) {
        vClip = vec4(0.0);
        vPrevClip = vec4(0.0);
        gl_Position = draw.viewProj * vec4(world, 1.0);
        gl_Position.z = max(gl_Position.z, -gl_Position.w); // pancaked; see cascadeShadowMap_csm.vs
        return;
    }
    vClip = matrices.PVNoJitter * vec4(world, 1.0);
    vPrevClip = matrices.prevPVNoJitter * vec4(still + sfeWindSwayTree(still, instance.posScale.xyz, matrices.windBefore), 1.0);
    gl_Position = matrices.PV * vec4(world, 1.0);
}

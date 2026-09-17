#version 450 core
#include "sfe.glsl"
#define SFE_WIND_TREES_ONLY
#include "sfe_wind.glsl"
#include "sfe_grass.glsl"
// One of GrassPass's grass clumps: a kit model placed, turned and scaled by its instance, bending in the gusts the
// trees sway in -- more the higher up the clump -- and pushed away from the character's feet. Into a shadow map only
// its position.
layout(location = 0) in vec3 aPos; // centimetres
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

layout(location = 0) out vec3 vWorld;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUv;
layout(location = 3) out vec4 vClip;
layout(location = 4) out vec4 vPrevClip;
layout(location = 5) flat out float vShade;

// Mirrors GrassPass::ClumpInstance.
struct SfeGrassClump {
    vec4 posScale; // xyz where it is rooted, w its scale
    vec4 yawShade; // x yaw in radians, y a brightness, z a phase for its sway, w its mesh
};
layout(SFE_SET(0, 21), std430) readonly buffer GrassClumps {
    SfeGrassClump clumps[];
};

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

// Mirrors GrassPass::ClumpDrawParams.
SFE_PUSH_BLOCK GrassClumpDraw {
    mat4 viewProj; // a light's, into a shadow map
    vec4 params;   // x 1 into a light's view, y and z the distances it shrinks away between
} draw;

// How far a point this far above the root is carried by the wind.
vec2 windPush(vec2 root, vec4 wind, float phase, float height) {
    float gust = sfeWindGust(root, wind);
    vec2 lean = wind.xz * wind.y * (gust * 0.9 + 0.3 * sin(wind.w * 2.4 + phase));
    return lean * height * height * 1.6;
}

void main() {
    SfeGrassClump clump = clumps[SFE_INSTANCE_ID];
    vec3 root = clump.posScale.xyz;
    float distanceToCamera = length(root.xz - grass.camera.xz);
    float scale = clump.posScale.w * (1.0 - smoothstep(draw.params.y, draw.params.z, distanceToCamera));
    float c = cos(clump.yawShade.x);
    float s = sin(clump.yawShade.x);
    vec3 local = aPos * scale;
    local = vec3(local.x * c - local.z * s, local.y, local.x * s + local.z * c);
    float height = max(local.y, 0.0);
    float phase = clump.yawShade.z;

    vec2 away = vec2(0.0);
    if (grass.trample.w > 0.0) {
        vec2 fromFeet = root.xz - grass.trample.xz;
        float reach = grass.trample.w + 0.3;
        float pressed = (1.0 - smoothstep(reach * 0.3, reach, length(fromFeet))) * (1.0 - smoothstep(0.5, 1.5, abs(root.y - grass.trample.y)));
        away = (length(fromFeet) > 1e-3 ? fromFeet / length(fromFeet) : vec2(1.0, 0.0)) * pressed * height * 1.2;
        local.y *= 1.0 - 0.45 * pressed;
    }
    vec2 push = windPush(root.xz, matrices.wind, phase, height) + away;
    vec2 pushBefore = windPush(root.xz, matrices.windBefore, phase, height) + away;
    vec3 world = root + local + vec3(push.x, 0.0, push.y);

    vWorld = world;
    vUv = aUv;
    vShade = clump.yawShade.y;
    vNormal = vec3(aNormal.x * c - aNormal.z * s, aNormal.y, aNormal.x * s + aNormal.z * c);
    if (draw.params.x > 0.5) {
        vClip = vec4(0.0);
        vPrevClip = vec4(0.0);
        gl_Position = draw.viewProj * vec4(world, 1.0);
        gl_Position.z = max(gl_Position.z, -gl_Position.w); // pancaked; see cascadeShadowMap_csm.vs
        return;
    }
    vec3 worldBefore = root + local + vec3(pushBefore.x, 0.0, pushBefore.y);
    vClip = matrices.PVNoJitter * vec4(world, 1.0);
    vPrevClip = matrices.prevPVNoJitter * vec4(worldBefore, 1.0);
    gl_Position = matrices.PV * vec4(world, 1.0);
}

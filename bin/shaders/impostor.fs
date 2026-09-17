#version 450 core
#include "sfe.glsl"
#include "sfe_impostor.glsl"
// An impostor into the G-buffer: the baked albedo and normal of the view it shows, the normal turned with the tree.
layout(location = 0) in vec2 vUv;
layout(location = 1) flat in vec4 vFrame0;
layout(location = 2) flat in vec4 vFrame1;
layout(location = 3) flat in vec3 vBlendYaw;
layout(location = 4) in vec3 vWorld;
layout(location = 5) in vec4 vClip;
layout(location = 6) in vec4 vPrevClip;

layout(location = 0) out vec4 gMaterial;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;
layout(location = 3) out vec2 gVelocity;

layout(SFE_SET(0, 23)) uniform sampler2D impostorAlbedo;
layout(SFE_SET(0, 24)) uniform sampler2D impostorNormal;

void main() {
    // Both views at once, weighted by how near each is: where they cover the same leaf the colours mix, and where
    // only one does it has to cover enough of the pixel on its own to keep it.
    vec2 uv0 = vFrame0.xy + vUv * vFrame0.zw;
    vec2 uv1 = vFrame1.xy + vUv * vFrame1.zw;
    vec4 albedo0 = textureLod(impostorAlbedo, uv0, 0.0);
    vec4 albedo1 = textureLod(impostorAlbedo, uv1, 0.0);
    float weight0 = albedo0.a * (1.0 - vBlendYaw.x);
    float weight1 = albedo1.a * vBlendYaw.x;
    float coverage = weight0 + weight1;
    if (coverage < 0.5) {
        discard;
    }
    vec4 albedo = vec4((albedo0.rgb * weight0 + albedo1.rgb * weight1) / coverage, coverage);
    // The nearer view's normal: the same for the whole picture, and near enough for lighting.
    vec4 normalSample = textureLod(impostorNormal, vBlendYaw.x < 0.5 ? uv0 : uv1, 0.0);
    vec3 n = normalSample.xyz * 2.0 - 1.0;
    float c = vBlendYaw.y;
    float s = vBlendYaw.z;
    vec3 N = normalize(vec3(n.x * c + n.z * s, n.y, -n.x * s + n.z * c));

    gMaterial = sfeGBufferMaterial(0.0, 0.12, 0.0, kSfeTranslucentFoliage);
    gNormal = vec4(N, gl_FragCoord.z / gl_FragCoord.w);
    // Darkened by the occlusion baked in with the normal: the G-buffer has nowhere else to keep it.
    gAlbedoSpec = vec4(pow(albedo.rgb, vec3(2.2)) * normalSample.a, 0.85);
    gVelocity = sfeVelocity(vClip, vPrevClip);
}

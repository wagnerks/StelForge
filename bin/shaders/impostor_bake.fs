#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUv;
layout(location = 2) in float vOcclusion;

layout(location = 0) out vec4 outAlbedo; // gamma encoded, a coverage
layout(location = 1) out vec4 outNormal; // the model's space, 0..1; a how much of the sky and sun the crown lets in

layout(SFE_SET(0, 1)) uniform sampler2D texture_diffuse1;

SFE_PUSH_BLOCK ImpostorBake {
    vec4 view;
    vec4 frame;
    vec4 tint;
} bake;

void main() {
    vec4 diffuse = texture(texture_diffuse1, vUv);
    if (bake.tint.a > 0.0 && diffuse.a < bake.tint.a) {
        discard;
    }
    vec3 linear = pow(diffuse.rgb, vec3(2.2)) * bake.tint.rgb;
    outAlbedo = vec4(pow(linear, vec3(1.0 / 2.2)), 1.0);
    outNormal = vec4(normalize(vNormal) * 0.5 + 0.5, vOcclusion);
}

#version 450 core
#include "sfe.glsl"
#include "sfe_impostor.glsl"
// An impostor into a shadow map: only where the picture is solid.
layout(location = 0) in vec2 vUv;
layout(location = 1) flat in vec4 vFrame0;

layout(SFE_SET(0, 23)) uniform sampler2D impostorAlbedo;

void main() {
    if (textureLod(impostorAlbedo, vFrame0.xy + vUv * vFrame0.zw, 0.0).a < 0.5) {
        discard;
    }
}

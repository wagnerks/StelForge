#version 450 core
#include "sfe.glsl"
// A grass clump into a shadow map: only where the atlas is solid.
layout(location = 2) in vec2 vUv;

layout(SFE_SET(0, 22)) uniform sampler2D atlas;

void main() {
    if (texture(atlas, vUv).a < 0.5) {
        discard;
    }
}

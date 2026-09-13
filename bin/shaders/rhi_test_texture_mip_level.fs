#version 450 core
#include "sfe.glsl"

// Copies one mip level of a texture into a target of that level's size. The target's pixel
// centres are the level's texel centres, so vUv lands on one texel and the lookup returns it whole.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2D testTexture;

layout(SFE_SET(0, 1), std140) uniform TestMipLevelBlock {
    vec4 params; // x is the lod to read; yzw unused
} block;

void main() {
    fragColor = textureLod(testTexture, vUv, block.params.x);
}

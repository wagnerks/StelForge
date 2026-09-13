#version 450 core
#include "sfe.glsl"

// Nothing but the sampled texel: no tint, no swizzle, no second term to hide a wrong lookup behind.
// The test picks the UVs, so whatever comes out of here is the texel the backend chose.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2D testTexture;

void main() {
    fragColor = texture(testTexture, vUv);
}

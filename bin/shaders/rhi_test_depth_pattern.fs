#version 450 core
#include "sfe.glsl"

// A depth texture cannot be created with initial data on Vulkan, so the pattern is rendered: every
// fragment writes the depth of the 8x8 texel its vUv falls in. Neighbouring texels always differ
// by at least 1/8, which makes every 2x2 compare footprint mix passing and failing texels.
// Values are odd sixteenths, exact in D32_SFLOAT. The test rebuilds the same formula on the CPU.
layout(location = 0) in vec2 vUv;

const float kTexels = 8.0;

void main() {
    vec2 texel = floor(vUv * kTexels);
    gl_FragDepth = (mod(texel.x * 5.0 + texel.y * 3.0, kTexels) + 0.5) / kTexels;
}

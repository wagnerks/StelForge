#version 450 core
#include "sfe.glsl"

// Each layer of a depth array lands in its own channel, so one draw shows all three layers at once.
// Plain sampler2DArray, no comparison: the raw stored depth is the answer.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2DArray depthLayers;

void main() {
    fragColor = vec4(
        texture(depthLayers, vec3(vUv, 0.0)).r,
        texture(depthLayers, vec3(vUv, 1.0)).r,
        texture(depthLayers, vec3(vUv, 2.0)).r,
        1.0);
}

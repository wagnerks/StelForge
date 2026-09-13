#version 450 core
#include "sfe.glsl"

// One cube sample per draw: the direction and the explicit lod both arrive as push constants, so a
// test can walk the six faces without rebuilding anything between draws.
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform samplerCube testCube;

SFE_PUSH_BLOCK CubeProbePush {
    vec4 dirLod; // xyz is the sample direction; w is the lod
} probe;

void main() {
    // textureLod, not texture: the direction is the same for every fragment, so the implicit
    // derivatives are zero and an implicit lookup could never reach a level above 0.
    fragColor = textureLod(testCube, probe.dirLod.xyz, probe.dirLod.w);
}

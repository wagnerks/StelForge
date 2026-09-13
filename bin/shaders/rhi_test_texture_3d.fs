#version 450 core
#include "sfe.glsl"

// One volume sample per draw: the point in texture space arrives as a push constant, so a test can
// walk the depth slices without rebuilding anything between draws.
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler3D testVolume;

SFE_PUSH_BLOCK VolumeProbePush {
    vec4 uvw; // xyz is the sample point; w is unused
} probe;

void main() {
    // textureLod, not texture: every fragment samples the same point, so the implicit derivatives
    // are zero and the level the lookup lands on would be a property of the driver, not of the test.
    fragColor = textureLod(testVolume, probe.uvw.xyz, 0.0);
}

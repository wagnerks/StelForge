#version 450 core
#include "sfe.glsl"

// One binding carrying three textures. Each element is read on its own line and lands in its own
// channel, so an array delivered only up to element 0 — or only up to element 1 — produces a pixel
// no correct binding can produce, rather than a plausible colour that hides the gap.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 0)) uniform sampler2D testTextures[3];

void main() {
    // Constant indices. Indexing a sampler array with a value that is not dynamically uniform is
    // undefined on both APIs, and this test is about whether the elements arrived, not about that
    // rule. The textures are solid, so vUv cannot influence the answer either.
    fragColor = vec4(
        texture(testTextures[0], vUv).r,
        texture(testTextures[1], vUv).r,
        texture(testTextures[2], vUv).r,
        1.0);
}

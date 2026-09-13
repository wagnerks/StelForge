#version 450 core
#include "sfe.glsl"

// Both names appear in OpenGLRHI's post-link sampler table, which assigns each of them unit 1. The
// declared bindings disagree with that table on purpose: only worldPos is read, and it is declared
// at binding 2.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(SFE_SET(0, 1)) uniform sampler2D historyColor;
layout(SFE_SET(0, 2)) uniform sampler2D worldPos;

void main() {
    fragColor = texture(worldPos, vec2(0.5));
}

#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec2 TexCoords;
layout(location = 0) out vec4 color;

layout(SFE_SET(0, 24)) uniform sampler2D text;

SFE_PUSH_BLOCK Push {
    vec3 textColor;
} pc;

void main() {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(pc.textColor, 1.0) * sampled;
}

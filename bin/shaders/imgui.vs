#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in uint aColor;

layout(location = 0) out vec2 Frag_UV;
layout(location = 1) out vec4 Frag_Color;

SFE_PUSH_BLOCK PushConstants {
	mat4 proj;
} pc;

void main() {
	Frag_UV = aUV;
	Frag_Color = unpackUnorm4x8(aColor);
	gl_Position = pc.proj * vec4(aPos, 0.0, 1.0);
}

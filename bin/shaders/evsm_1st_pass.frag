#version 400 core

uniform vec2 u_Exponents;

layout (location = 0) out vec4 depthMap;

in vec4 v_LightSpacePos;

void main(void) {

	float depth = v_LightSpacePos.z / v_LightSpacePos.w;
	// ===================================================
	float pos = exp(u_Exponents.x * depth);
	// ===================================================

	depthMap = vec4(pos, pos * pos, 0.f, 0.f);


}

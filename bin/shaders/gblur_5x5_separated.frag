#version 400 core

uniform vec2  u_TexelSize;

uniform sampler2D u_InputTexture;

layout (location = 0) out vec4 FragColor;

in vec2 v_TexCoords;

// Kernel generated at: http://dev.theomader.com/gaussian-kernel-calculator/
const float blurKernel[5] = float[](
//0.06136,	0.24477,	0.38774,	0.24477,	0.06136 // sigma = 1.0
0.153388,	0.221461,	0.250301,	0.221461,	0.153388 // sigma = 2.0
);

void main(void) {
	vec3 finalColor = vec3(0.0);
	for (int i = -2; i <= 2; i++) {
		finalColor += texture(u_InputTexture, v_TexCoords.st + i * u_TexelSize).xyz * blurKernel[i + 2];
	}
	FragColor = vec4(finalColor, 1.0);
}

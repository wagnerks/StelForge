#version 400 core

uniform vec2  u_TexelSize;

uniform sampler2D u_InputTexture;

layout (location = 0) out vec4 FragColor;

in vec2 v_TexCoords;

// Kernel generated at: http://dev.theomader.com/gaussian-kernel-calculator/
const float blurKernel[7] = float[](
//0.00598,	0.060626,	0.241843,	0.383103,	0.241843,	0.060626,	0.00598 // sigma = 1.0
0.071303,	0.131514,	0.189879,	0.214607,	0.189879,	0.131514,	0.071303 // sigma = 2.0
);

void main(void) {
	vec3 finalColor = vec3(0.0);
	for (int i = -3; i <= 3; i++) {
		finalColor += texture(u_InputTexture, v_TexCoords.st + i * u_TexelSize).xyz * blurKernel[i + 3];
	}
	FragColor = vec4(finalColor, 1.0);
}

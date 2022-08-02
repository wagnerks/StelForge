#version 400 core

uniform vec2  u_TexelSize;

uniform sampler2D u_InputTexture;

layout (location = 0) out vec4 FragColor;

in vec2 v_TexCoords;

// Kernel from: https://computergraphics.stackexchange.com/questions/39/how-is-gaussian-blur-implemented
// I presume it is approximate using the Pascal pyramid
//const float blurKernel[25] = float[](
//    1.0 / 256.0,  4.0 / 256.0,  6.0 / 256.0,  4.0 / 256.0, 1.0 / 256.0,
//	4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0,
//	6.0 / 256.0, 24.0 / 256.0, 36.0 / 256.0, 24.0 / 256.0, 6.0 / 256.0,
//	4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0,
//    1.0 / 256.0,  4.0 / 256.0,  6.0 / 256.0,  4.0 / 256.0, 1.0 / 256.0
//);

// Kernel generated at: http://dev.theomader.com/gaussian-kernel-calculator/
const float blurKernel[25] = float[](
0.023528,	0.033969,	0.038393,	0.033969,	0.023528,
0.033969,	0.049045,	0.055432,	0.049045,	0.033969,
0.038393,	0.055432,	0.062651,	0.055432,	0.038393,
0.033969,	0.049045,	0.055432,	0.049045,	0.033969,
0.023528,	0.033969,	0.038393,	0.033969,	0.023528
);


void main(void) {
	vec3 finalColor = vec3(0.0);
	for (int x = -2; x <= 2; x++) {
		for (int y = -2; y <= 2; y++) {
			finalColor += texture(u_InputTexture, vec2(v_TexCoords.x + u_TexelSize.x * x, v_TexCoords.y + u_TexelSize.y * y)).rgb * blurKernel[x + 2 + (y + 2) * 5];
		}
	}
	FragColor = vec4(finalColor, 1.0);
}

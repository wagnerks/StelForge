#version 400 core

uniform sampler2D u_InputTexture;
uniform vec2 u_TexelSize;

out float FragColor;

in vec2 v_TexCoords;

//const float blurKernel[9] = float[](
//    1.0 / 16, 2.0 / 16, 1.0 / 16,
//    2.0 / 16, 4.0 / 16, 2.0 / 16,
//    1.0 / 16, 2.0 / 16, 1.0 / 16  
//);

const float blurKernel[9] = float[](
0.102059,	0.115349,	0.102059,
0.115349,	0.130371,	0.115349,
0.102059,	0.115349,	0.102059
);

void main(void) {
	vec3 finalColor = vec3(0.0);
	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			finalColor += vec3(texture(u_InputTexture, vec2(v_TexCoords.x + u_TexelSize.x * x, v_TexCoords.y + u_TexelSize.y * y))) * blurKernel[x + 1 + (y + 1) * 3];
		}
	}
	FragColor = finalColor.r;
}

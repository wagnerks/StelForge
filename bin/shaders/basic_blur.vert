#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 v_TexCoords;
out vec2 pos;
out vec3 posCoord;
void main() {
	gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
	v_TexCoords = aTexCoords;
	pos = aPos.xy;
	posCoord = aPos.xyz;
}

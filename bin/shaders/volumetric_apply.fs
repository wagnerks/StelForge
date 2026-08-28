#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneHdr;
layout(binding = 1) uniform sampler2D volumeLight;

void main() {
    vec3 scene = texture(sceneHdr, TexCoords).rgb;
    vec4 vol = texture(volumeLight, TexCoords);
    FragColor = vec4(scene * vol.a + vol.rgb, 1.0);
}

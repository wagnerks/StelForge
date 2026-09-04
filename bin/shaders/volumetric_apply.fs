#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D sceneHdr;
layout(SFE_SET(0, 1)) uniform sampler2D volumeLight;

void main() {
    vec3 scene = texture(sceneHdr, TexCoords).rgb;
    vec4 vol = texture(volumeLight, TexCoords);
    FragColor = vec4(scene * vol.a + vol.rgb, 1.0);
}

#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D sceneHdr;
layout(SFE_SET(0, 1)) uniform sampler2D worldPos;
layout(SFE_SET(0, 2)) uniform sampler2D gAlbedoSpec;
layout(SFE_SET(0, 3)) uniform sampler2D sheenNormal;

layout(SFE_SET(0, 15), std140) uniform PushConstants {
    vec3 lightDir;
    float sheenStrength;
    vec3 lightColor;
    float rimStrength;
    vec3 viewPos;
    float wrapStrength;
} params;

void main() {
    FragColor = vec4(texture(sceneHdr, TexCoords).rgb, 1.0);
}

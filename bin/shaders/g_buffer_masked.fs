#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in mat3 TBN;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;

layout(SFE_SET(1, 1)) uniform sampler2D texture_diffuse1;
layout(SFE_SET(1, 2)) uniform sampler2D normalMap;
layout(SFE_SET(1, 3)) uniform sampler2D texture_specular1;
layout(SFE_SET(1, 4)) uniform sampler2D texture_roughness;
layout(SFE_SET(1, 5)) uniform sampler2D texture_metallic;

SFE_PUSH_BLOCK PushConstants {
    float matRoughness;
    float matMetallic;
    float pad0;
    float pad1;
} pc;

void main() {
    vec4 diff = texture(texture_diffuse1, TexCoords, -0.75);
    if (diff.a < 0.4) {
        discard;
    }

    gPosition.rgb = FragPos;
    gNormal.a = gl_FragCoord.z / gl_FragCoord.w;

    vec3 nSample = texture(normalMap, TexCoords).rgb;
    if (dot(nSample, nSample) < 0.02) {
        gNormal.xyz = normalize(TBN[2]);
    } else {
        vec3 nts = nSample * 2.0 - 1.0;
        gNormal.xyz = normalize(TBN * nts);
    }

    gAlbedoSpec.rgb = pow(diff.rgb, vec3(2.2));

    float spec = texture(texture_specular1, TexCoords).r;
    float roughMap = texture(texture_roughness, TexCoords).g;
    if (roughMap < 0.001) {
        roughMap = texture(texture_roughness, TexCoords).r;
    }
    float roughness = pc.matRoughness;
    if (roughMap > 0.001) {
        roughness = roughMap;
    } else if (spec > 0.001) {
        roughness = 1.0 - spec;
    }
    float metallic = texture(texture_metallic, TexCoords).b;
    if (metallic < 0.001) {
        metallic = texture(texture_metallic, TexCoords).r;
    }
    if (metallic < 0.001) {
        metallic = pc.matMetallic;
    }

    gAlbedoSpec.a = clamp(roughness, 0.04, 1.0);
    gPosition.a = clamp(metallic, 0.0, 1.0);
}

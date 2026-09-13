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
    vec3 hdr = texture(sceneHdr, TexCoords).rgb;
    vec4 posS = texture(worldPos, TexCoords);
    vec3 nSample = texture(sheenNormal, TexCoords).rgb;
    if (dot(nSample, nSample) < 0.01 || dot(posS.rgb, posS.rgb) < 1e-8) {
        FragColor = vec4(hdr, 1.0);
        return;
    }

    vec4 albS = texture(gAlbedoSpec, TexCoords);
    vec3 alb = albS.rgb;
    float rough = clamp(albS.a, 0.04, 1.0);
    vec4 pbr = sfeGBufferPbr(posS.a);
    float metallic = pbr.x;
    float clearcoat = pbr.z;
    float fabric = (1.0 - metallic) * mix(0.15, 0.85, rough) * (1.0 - clearcoat);
    if (fabric < 0.02 || params.sheenStrength < 0.01) {
        FragColor = vec4(hdr, 1.0);
        return;
    }

    vec3 N = normalize(nSample);
    vec3 V = normalize(params.viewPos - posS.rgb);
    vec3 L = normalize(-params.lightDir);
    float wrap = max(params.wrapStrength, 0.0);
    float wrapL = clamp((dot(N, L) + wrap) / max(1.0 + wrap, 1e-3), 0.0, 1.0);
    float ndv = max(dot(N, V), 0.0);
    float rim = pow(1.0 - ndv, 3.0) * params.rimStrength;
    float lum = dot(alb, vec3(0.2126, 0.7152, 0.0722));
    vec3 sheen = vec3(lum) * params.lightColor * (wrapL * 0.12 + rim * 0.04) * params.sheenStrength * fabric;
    FragColor = vec4(hdr + sheen, 1.0);
}

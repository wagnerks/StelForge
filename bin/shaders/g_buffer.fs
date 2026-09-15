#version 450 core
#include "sfe.glsl"
#include "sfe_material.glsl"
layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in mat3 TBN;
layout(location = 5) in vec2 TexCoords1;
layout(location = 6) flat in uint vMaterial;
layout(location = 7) in vec4 vClip;
layout(location = 8) in vec4 vPrevClip;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;
layout(location = 3) out vec2 gVelocity;

layout(SFE_SET(1, 1)) uniform sampler2D texture_diffuse1;
layout(SFE_SET(1, 2)) uniform sampler2D normalMap;
layout(SFE_SET(1, 3)) uniform sampler2D texture_specular1;
layout(SFE_SET(1, 4)) uniform sampler2D texture_roughness;
layout(SFE_SET(1, 5)) uniform sampler2D texture_metallic;
layout(SFE_SET(1, 6)) uniform sampler2D texture_subsurface;

#define pc sfeMaterials[vMaterial]

vec2 uvOf(uint bit) {
    return ((uint(pc.uvSel + 0.5) & bit) != 0u) ? TexCoords1 : TexCoords;
}

void main() {
    uint maps = uint(pc.mapMask + 0.5);
    vec2 uvDiff = uvOf(1u);
    vec2 uvNrm = uvOf(2u);
    vec2 uvSpec = uvOf(4u);
    vec2 uvRough = uvOf(8u);
    vec2 uvMetal = uvOf(16u);
    vec2 uvSss = uvOf(32u);

    // The mip the pixel would sample: reading the mask at level 0 made the coverage of a distant
    // character depend on which single texel the jitter landed on.
    float diffLod = textureQueryLod(texture_diffuse1, uvDiff).y;
    float mask = textureLod(texture_diffuse1, uvDiff, diffLod).a;
    if ((maps & 8u) != 0u && mask > 0.99) {
        mask = max(texture(texture_specular1, uvSpec).r, texture(texture_roughness, uvRough).g);
    }
    uint uvBits = uint(pc.uvSel + 0.5);
    if ((uvBits & 1u) == 0u && (uvBits & 6u) != 0u) {
        mask = min(mask, texture(texture_diffuse1, TexCoords1).a);
    }
    if (pc.alphaCutoff > 1.0) {
        float cov = clamp(pc.alphaCutoff - 1.0, 0.05, 1.0);
        mask = min(mask, cov);
        float d = sfeCutoutDither(texture_diffuse1, uvDiff, diffLod);
        if (mask < d) {
            discard;
        }
    } else if (pc.alphaCutoff > 0.0 && mask < pc.alphaCutoff) {
        discard;
    }
    vec4 diff = texture(texture_diffuse1, uvDiff);
    if (pc.alphaCutoff > 0.0) {
        diff.rgb = sfeMaskedAlbedo(texture_diffuse1, uvDiff);
    }

    gPosition.rgb = FragPos;
    gNormal.a = gl_FragCoord.z / gl_FragCoord.w;

    vec3 nSample = texture(normalMap, uvNrm).rgb;
    vec3 Nw;
    if (dot(nSample, nSample) < 0.02) {
        Nw = normalize(TBN[2]);
    } else {
        vec3 nts = nSample * 2.0 - 1.0;
        Nw = normalize(TBN * nts);
    }
    if (pc.clearcoat > 0.5) {
        Nw = normalize(TBN[2]);
    } else if (pc.clearcoat > 0.01) {
        Nw = normalize(mix(Nw, normalize(TBN[2]), pc.clearcoat));
    }
    float sss = texture(texture_subsurface, uvSss).r;
    if (sss < 0.001) {
        sss = texture(texture_subsurface, uvSss).a;
    }
    vec3 linAlb = pow(diff.rgb, vec3(2.2));
    if (sss < 0.05 && pc.clearcoat < 0.5) {
        sss = max(sss, sfeSkinSss(linAlb, 0.0));
    }
    const bool cutout = pc.alphaCutoff > 0.0;
    if (cutout) {
        Nw = normalize(TBN[2]);
        sss = 0.0;
    }
    gNormal.xyz = Nw * (1.0 - 0.5 * clamp(sss, 0.0, 1.0));

    gAlbedoSpec.rgb = linAlb;

    float roughness = pc.matRoughness;
    if ((maps & 1u) != 0u) {
        roughness = texture(texture_roughness, uvRough).g;
    } else if ((maps & 4u) != 0u) {
        roughness = 1.0 - texture(texture_specular1, uvSpec).r;
    }
    roughness = mix(roughness, max(roughness, 0.62), clamp(sss, 0.0, 1.0));
    if (pc.clearcoat > 0.5) {
        roughness = 0.48;
    }
    float metallic = pc.matMetallic;
    if ((maps & 2u) != 0u) {
        metallic = texture(texture_metallic, uvMetal).b;
    }
    metallic *= 1.0 - clamp(pc.clearcoat, 0.0, 1.0);
    if ((maps & 8u) != 0u || cutout) {
        metallic = 0.0;
    }

    float specF = pc.specFactor;
    if ((maps & 1u) != 0u && (maps & 4u) != 0u) {
        specF *= texture(texture_specular1, uvSpec).r;
    }
    if (cutout) {
        roughness = max(roughness, 0.78);
        specF = min(specF, 0.18);
    }

    gAlbedoSpec.a = clamp(roughness, 0.04, 1.0);
    gPosition.a = sfePackGBufferPbr(clamp(metallic, 0.0, 1.0), clamp(specF, 0.0, 1.0), clamp(pc.clearcoat, 0.0, 1.0), clamp(pc.clearcoatRough, 0.04, 1.0));
    gVelocity = sfeVelocity(vClip, vPrevClip);
}

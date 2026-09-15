// Material parameters, one entry per distinct material. Mirrors SFE::Render::GpuMaterial; the pass
// rules (dither cutoff, caster opacity, glass defaults) are resolved on the CPU when an entry is made.
struct SfeMaterial {
    float matRoughness;
    float matMetallic;
    float mapMask;
    float alphaCutoff;     // g-buffer: > 1 dithers
    float uvSel;
    float specFactor;
    float clearcoat;
    float clearcoatRough;
    vec3 tint;
    float opacity;         // glass density
    float ior;             // glass
    float casterOpacity;   // shadow casters
    float shadowCutoff;    // shadow casters: < 0 takes the mask from r/g
    float albedoSlot;      // glass: index into the glass albedo array
};

layout(SFE_SET(0, 7), std430) readonly buffer SfeMaterials {
    SfeMaterial sfeMaterials[];
};

// Fragment only: it needs the mip the rasterizer would have picked.
vec3 sfeMaskedAlbedo(sampler2D tex, vec2 uv) {
    // A cutout texture keeps junk under its transparent texels, so a pixel on the edge takes the colour of
    // the nearest texel that is actually there. Everything here happens at the mip this pixel would have
    // sampled anyway: done at level 0, the search picked a different texel every frame once the model was
    // small on screen, and TAA averaged that flicker into grey.
    vec4 base = texture(tex, uv);
    if (base.a > 0.5) {
        return base.rgb;
    }
    float lod = textureQueryLod(tex, uv).y;
    vec2 texel = exp2(lod) / vec2(textureSize(tex, 0));
    vec2 o[4] = vec2[](vec2(texel.x, 0.0), vec2(-texel.x, 0.0), vec2(0.0, texel.y), vec2(0.0, -texel.y));
    vec3 bestRgb = base.rgb;
    float best = base.a;
    for (int i = 0; i < 4; ++i) {
        vec4 s = textureLod(tex, uv + o[i], lod);
        if (s.a > best) {
            best = s.a;
            bestRgb = s.rgb;
        }
    }
    return bestRgb;
}

// The stipple a dithered cutout discards by. Anchored to the texel grid of the mip this pixel samples, so
// it does not change when TAA jitters the sample a fraction of a pixel: a key that moved with the sample
// gave every frame a different pattern, which is what made the characters crawl and average out to grey.
float sfeCutoutDither(sampler2D tex, vec2 uv, float lod) {
    // floor(lod): the exact level of detail differs a little between the two backends, and a key that
    // followed it flipped single stipple pixels from one to the other.
    precise vec2 key = floor(uv * vec2(textureSize(tex, 0)) / exp2(floor(lod)));
    return sfeHash12(key);
}

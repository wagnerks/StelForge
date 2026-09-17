#include "sfe_material_table.glsl"

// How rough this surface has to be written down as for the highlight to survive being squeezed into one
// pixel. Two things spread the normal inside a pixel footprint and both read as a wider highlight: the
// normal map's own texels, averaged into one sample by the mip (a shortened normal is exactly that
// average), and the geometry curving across the footprint. Both are variances, and filtering a GGX lobe
// with a gaussian of variance s adds 2s to alpha squared. Without this a distant character sparkles:
// the highlight lands in a different texel every frame and no amount of temporal filtering can average
// a signal that never stays put.
float sfeFilteredRoughness(vec3 worldNormal, float roughness, float normalMapLength) {
    vec3 dndx = dFdx(worldNormal);
    vec3 dndy = dFdy(worldNormal);
    float geometric = 0.25 * 0.5 * (dot(dndx, dndx) + dot(dndy, dndy));
    float len = clamp(normalMapLength, 1e-3, 1.0);
    float fromMap = (1.0 - len) / (2.0 * len);
    // Capped: past this the surface is being described as rougher than it is, and a character in the
    // middle distance turns flat and matte instead of stopping sparkling.
    float variance = min(geometric + fromMap, 0.09);
    float alpha = roughness * roughness;
    return sqrt(sqrt(min(1.0, alpha * alpha + 2.0 * variance)));
}

// Fragment only: it needs the mip the rasterizer would have picked.
vec3 sfeMaskedAlbedo(sampler2D tex, vec2 uv, vec4 base) {
    // A cutout texture keeps junk under its transparent texels, so a pixel on the edge takes the colour of
    // the nearest texel that is actually there. Everything here happens at the mip this pixel would have
    // sampled anyway: done at level 0, the search picked a different texel every frame once the model was
    // small on screen, and TAA averaged that flicker into grey.
    // `base` is texture(tex, uv), which the caller has already read.
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

// How much to stop believing the alpha channel. Near the top of the mip chain one texel of the map covers
// a large part of the surface, so its alpha is an average of holes and cloth rather than a statement about
// this pixel; testing against it there turns a silhouette into noise that changes with every small camera
// move. Ramped over the last two levels, the cutout closes up into the solid shape it is nearly covering
// anyway, which holds still.
float sfeCutoutFade(sampler2D tex, float lod) {
    float top = float(textureQueryLevels(tex) - 1);
    return clamp((lod - (top - 2.0)) * 0.5, 0.0, 1.0);
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

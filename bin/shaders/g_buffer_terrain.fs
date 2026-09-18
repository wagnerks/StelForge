#version 450 core
#include "sfe.glsl"
#include "sfe_material.glsl"
#include "sfe_terrain.glsl"
// Ground painted from where it is with tiling materials, in their own colours: grass, bare earth and the road, roots
// under the trees, the bank and the lake bed, rock where it is steep. The terrain mesh puts world x/z in metres into
// TexCoords and the height above the water line into TexCoords1.x.
layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in mat3 TBN;
layout(location = 5) in vec2 TexCoords1;
layout(location = 6) flat in uint vMaterial;
layout(location = 7) in vec4 vClip;
layout(location = 8) in vec4 vPrevClip;

layout(location = 0) out vec4 gMaterial;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;
layout(location = 3) out vec2 gVelocity;

// Layer per material, see bin/textures/ground: rgb base colour and a height; rg normal, b roughness, a occlusion.
layout(SFE_SET(1, 1)) uniform sampler2DArray groundAlbedo;
layout(SFE_SET(1, 2)) uniform sampler2DArray groundNormal;
// VegetationPlacementsComponent::forestMap, in the material's SPECULAR slot.
layout(SFE_SET(1, 3)) uniform sampler2D forestMap;
// terrainNoiseTexture, in the ROUGHNESS slot.
layout(SFE_SET(1, 4)) uniform sampler2D terrainNoise;

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

const int kGrass = 0;
const int kMoss = 1;
const int kMud = 2;
const int kSand = 3;
const int kRocks = 4;
const int kLitter = 5;
const int kTrack = 6; // between the road's ruts
const int kLayers = 7;
// Metres one tile covers.
const float kTile[kLayers] = float[](2.5, 3.5, 3.0, 4.0, 3.0, 2.5, 3.0);
// The materials are painted bright; this brings them down to the light the rest of the scene is lit for.
const float kBrightness = 0.75;
// Metres the height map's full range stands for, for parallax: stones and clumps look raised, not painted on.
const float kRelief[kLayers] = float[](0.05, 0.06, 0.07, 0.05, 0.1, 0.05, 0.05);
// The painted maps are gentle; lit by a low sun they read as flat without some help.
const float kNormalStrength = 0.9;
// Hummocks, ruts and dips a metre or two across: the ground's own unevenness between the mesh's vertices.
const float kBumps[kLayers] = float[](0.35, 0.45, 0.3, 0.12, 0.5, 0.4, 0.3);

// How much of a noise of this frequency survives the pixel footprint: fades detail out before it aliases.
float detail(float frequency, float footprint) {
    return 1.0 - smoothstep(0.2, 0.6, frequency * footprint);
}

// The slope of a noise field along x and z, by forward differences.
vec2 noiseSlope(vec2 p, float frequency, int octaves) {
    const float e = 0.05;
    float c = sfeFbm(p * frequency, octaves);
    float dx = sfeFbm((p + vec2(e, 0.0)) * frequency, octaves) - c;
    float dz = sfeFbm((p + vec2(0.0, e)) * frequency, octaves) - c;
    return vec2(dx, dz) / e;
}

struct GroundSample {
    vec3 colour;  // linear
    vec3 normal;  // tangent space, z up
    float roughness;
    float occlusion;
    float height;
};

// One material at `p`, twice: the second sample turned and scaled against the first and faded in by a slow noise, so
// the tiles do not line up into a grid across a meadow. Gradients are given: only some materials are sampled, and
// the implicit ones are undefined inside that branch. Drawn, it is the colour alone: those materials are flat paint,
// their normal maps level, and nothing is left for the second sample or the normal map to add.
GroundSample sampleGround(int layer, vec2 p, vec2 dpdx, vec2 dpdy, float variation, vec3 viewTangent, float parallax, bool drawn) {
    const mat2 turn = mat2(0.6, 0.8, -0.8, 0.6);
    const float scale2 = 0.71;
    float tile = kTile[layer];
    // Parallax: one step along the view by the height under the pixel, so what stands up shifts towards the eye.
    if (parallax > 0.001) {
        float h = textureGrad(groundAlbedo, vec3(p / tile, layer), dpdx / tile, dpdy / tile).a;
        p += viewTangent.xy / max(viewTangent.z, 0.35) * (h - 0.5) * kRelief[layer] * parallax;
    }
    vec2 uv = p / tile;
    vec2 uv2 = turn * uv * scale2 + vec2(0.37, 0.81);
    vec2 dx = dpdx / tile;
    vec2 dy = dpdy / tile;
    vec2 dx2 = turn * dx * scale2;
    vec2 dy2 = turn * dy * scale2;
    vec3 at = vec3(uv, layer);
    vec3 at2 = vec3(uv2, layer);
    // Mostly one of the two is all there is: only the fade between them reads both.
    vec4 a1 = vec4(0.0);
    vec4 n1 = vec4(0.5, 0.5, 1.0, 1.0);
    vec4 a2 = vec4(0.0);
    vec4 n2 = vec4(0.5, 0.5, 1.0, 1.0);
    if (drawn) {
        a1 = textureGrad(groundAlbedo, at, dx, dy);
        variation = 0.0;
    }
    else if (variation < 0.999) {
        a1 = textureGrad(groundAlbedo, at, dx, dy);
        n1 = textureGrad(groundNormal, at, dx, dy);
    }
    if (!drawn && variation > 0.001) {
        a2 = textureGrad(groundAlbedo, at2, dx2, dy2);
        n2 = textureGrad(groundNormal, at2, dx2, dy2);
    }
    vec4 a = mix(a1, a2, variation);
    // The second sample's normal is in the turned frame: turn it back.
    vec2 xy = mix(n1.rg * 2.0 - 1.0, transpose(turn) * (n2.rg * 2.0 - 1.0), variation) * kNormalStrength;

    GroundSample s;
    s.colour = pow(a.rgb, vec3(2.2));
    s.normal = vec3(xy, sqrt(max(1.0 - dot(xy, xy), 0.0)));
    s.roughness = mix(n1.b, n2.b, variation);
    s.occlusion = mix(n1.a, n2.a, variation);
    s.height = a.a;
    return s;
}

void main() {
    vec2 p = TexCoords;
    vec2 dpdx = dFdx(p);
    vec2 dpdy = dFdy(p);
    float height = TexCoords1.x;
    vec3 N = normalize(TBN[2]);
    float slope = 1.0 - N.y;

    vec3 T = normalize(vec3(1.0, 0.0, 0.0) - N * N.x);
    vec3 B = normalize(vec3(0.0, 0.0, 1.0) - N * N.z);
    // The camera, from the rigid view matrix.
    vec3 camera = -transpose(mat3(matrices.view)) * matrices.view[3].xyz;
    vec3 toEye = normalize(camera - FragPos);
    vec3 viewTangent = vec3(dot(toEye, T), dot(toEye, B), dot(toEye, N));
    float footprint = length(fwidth(p));
    // The drawn look paints the ground flat: no relief, no hummocks, no ruts in the normal. Those were most of what
    // this shader cost.
    bool drawn = matrices.style.x > 0.5;
    // Parallax only up close, where a height map's centimetres still cover a few pixels.
    float parallax = drawn ? 0.0 : 1.0 - smoothstep(0.01, 0.04, footprint);

    vec4 noise = sfeTerrainNoise(terrainNoise, p);
    SfeTerrainLayers layers = sfeTerrainLayers(p, height, slope, sfeForest(forestMap, p), noise);
    float broad = layers.broad;
    float patches = layers.patches;
    float fine = sfeFbm(p * 0.35, 2);
    float variation = smoothstep(0.42, 0.58, noise.b);
    // A lighter and a darker tone of every colour, in soft blotches of a few metres.
    float blotchNoise = sfeFbm(p * 0.28 + 5.0, 2);
    float blotch = smoothstep(0.35, 0.65, blotchNoise);

    // How much of each material, before their heights decide the edges between them.
    float weight[kLayers];
    float underwater = layers.underwater;
    float steep = layers.rock * (1.0 - underwater);
    float open = (1.0 - steep) * (1.0 - underwater);
    float bank = clamp(layers.sand + layers.mud, 0.0, 1.0);
    float land = open * (1.0 - bank);
    float stony = smoothstep(0.55, 0.8, patches);
    weight[kGrass] = land * (1.0 - layers.earth) * (1.0 - layers.litter);
    weight[kLitter] = land * (1.0 - layers.earth) * layers.litter;
    weight[kMoss] = land * layers.earth;
    weight[kMud] = open * layers.mud + underwater * (1.0 - stony);
    weight[kSand] = open * layers.sand;
    weight[kRocks] = steep + underwater * stony;
    weight[kTrack] = 0.0;
    // The road: packed dirt, two wheel ruts, and a clovered strip down the middle between them.
    float roadAbs = abs(layers.roadDistance);
    float road = layers.road * (1.0 - underwater);
    float roadStrip = 1.0 - smoothstep(0.35, 0.65, roadAbs + 0.12 * (fine - 0.5));
    // Squared by hand: pow() of a negative base is undefined, and came out NaN on the inner side of each rut.
    float rutOffset = (roadAbs - 0.8) / 0.28;
    float rut = exp(-rutOffset * rutOffset);
    for (int i = 0; i < kLayers; ++i) {
        weight[i] *= 1.0 - road;
    }
    weight[kMoss] += road * (1.0 - roadStrip);
    weight[kTrack] += road * roadStrip;

    // Over each material's own colour: a lighter and a darker tone in blotches, the bank darkening as it gets wet, and
    // the road darker down its ruts.
    vec3 tint[kLayers];
    for (int i = 0; i < kLayers; ++i) {
        tint[i] = vec3(mix(0.9, 1.08, blotch) * kBrightness);
    }
    tint[kMud] *= mix(1.0, 0.55, layers.wet);
    tint[kSand] *= mix(1.0, 0.55, layers.wet);
    tint[kMoss] *= mix(1.0, 0.6, rut * 0.8 * road);

    GroundSample samples[kLayers];
    float blend[kLayers];
    float highest = 0.0;
    for (int i = 0; i < kLayers; ++i) {
        blend[i] = 0.0;
        if (weight[i] < 0.01) {
            continue;
        }
        samples[i] = sampleGround(i, p, dpdx, dpdy, variation, viewTangent, parallax, drawn);
        // Stones stand out of the mud, grass tufts out of the sand.
        blend[i] = weight[i] + samples[i].height * 0.3;
        highest = max(highest, blend[i]);
    }

    vec3 albedo = vec3(0.0);
    vec3 detailNormal = vec3(0.0);
    float roughness = 0.0;
    float occlusion = 0.0;
    float bumpiness = 0.0;
    float total = 0.0;
    for (int i = 0; i < kLayers; ++i) {
        if (weight[i] < 0.01) {
            continue;
        }
        float w = max(blend[i] - highest + 0.6, 0.0) * weight[i];
        GroundSample s = samples[i];
        albedo += s.colour * tint[i] * w;
        detailNormal += s.normal * w;
        roughness += s.roughness * w;
        occlusion += s.occlusion * w;
        bumpiness += kBumps[i] * w;
        total += w;
    }
    total = max(total, 1e-4);
    albedo /= total;
    roughness /= total;
    occlusion /= total;
    bumpiness /= total;
    detailNormal = normalize(detailNormal / total);

    // Slow variation over the whole terrain, so a meadow is not one flat tone.
    albedo *= 0.85 + 0.3 * broad;
    albedo *= mix(1.0, occlusion, 0.35);

    // The material's normal on the ground's own frame: u runs along +x and v along +z, and the maps are OpenGL ones,
    // green towards the top of the image, where v is smallest.
    // The hummocks first, on the mesh's normal, faded out where they would only shimmer.
    if (!drawn) {
        vec2 bumps = noiseSlope(p, 0.6, 3) * 0.6;
        float fineBumps = detail(2.4, footprint);
        if (fineBumps > 0.001) {
            bumps += noiseSlope(p, 2.4, 2) * 0.15 * fineBumps;
        }
        N = normalize(N - vec3(bumps.x, 0.0, bumps.y) * bumpiness * detail(0.6, footprint));
    }
    // The ruts are a few centimetres deep, too narrow for the mesh: tilt the normal across the road by their slope,
    // the road's direction from how the distance to it changes.
    if (!drawn && road > 0.01) {
        const float e = 0.5;
        float along = (sfeTerrainNoise(terrainNoise, p + vec2(e, 0.0)).a - noise.a) * kSfeRoadTextureRange / e;
        float across = (sfeTerrainNoise(terrainNoise, p + vec2(0.0, e)).a - noise.a) * kSfeRoadTextureRange / e;
        vec2 towards = vec2(along, across);
        float rutSlope = 0.05 * rut * 2.0 * (roadAbs - 0.8) / (0.28 * 0.28) * sign(layers.roadDistance);
        N = normalize(N - vec3(towards.x, 0.0, towards.y) * rutSlope * road * detail(1.5, footprint));
    }
    N = normalize(T * detailNormal.x - B * detailNormal.y + N * detailNormal.z);

    // Wet near the water: glossier, a little darker. Under it the water does the rest.
    albedo *= mix(1.0, 0.85, layers.wet * (1.0 - underwater));
    roughness = mix(roughness, 0.35, layers.wet * (1.0 - underwater));
    float specFactor = mix(0.08, 0.6, layers.wet);

    gNormal.xyz = N;
    gNormal.a = gl_FragCoord.z / gl_FragCoord.w;
    gAlbedoSpec.rgb = albedo;
    gAlbedoSpec.a = clamp(sfeFilteredRoughness(N, roughness, 1.0), 0.04, 1.0);
    gMaterial = sfeGBufferMaterial(0.0, specFactor, 0.0, 0.04);
    gVelocity = sfeVelocity(vClip, vPrevClip);
}

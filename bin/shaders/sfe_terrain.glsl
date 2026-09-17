#ifndef SFE_TERRAIN_GLSL
#define SFE_TERRAIN_GLSL
// What covers the ground where: the terrain shader paints by it, and grass grows by it, so the blades stand exactly
// where the ground under them is grass. Include after sfe.glsl.

struct SfeTerrainLayers {
    float broad;      // large-scale variation, 0..1
    float patches;    // mid-scale variation, 0..1
    float underwater; // weights, 0..1, bottom up
    float wet;
    float sand;       // the bank where it is sandy: a few coves
    float mud;        // the rest of the bank: dark silt and mud
    float rock;
    float earth;      // moss among the grass, and in patches on the forest floor
    float forest;     // how deep in the forest, 0..1, from the forest map
    float litter;     // fallen needles and twigs under the trees
    float roadDistance; // metres from the road's centreline, signed by its side
    float road;       // the road's surface
};

// Metres a texel of the forest map covers; VegetationComponent.h has the same.
const float kSfeForestTexelMetres = 2.0;

// Metres a texel of the terrain noise covers; Terrain.h has the same.
const float kSfeTerrainNoiseTexelMetres = 4.0;

// Metres of signed road distance the noise texture's alpha spans; Terrain.h has the same.
const float kSfeRoadTextureRange = 16.0;

// The terrain's slow noise at `p`, baked by terrainNoiseTexture: r broad variation, g sandy coves, b tile variation,
// a the signed distance to the road.
vec4 sfeTerrainNoise(sampler2D noiseMap, vec2 p) {
    float side = float(textureSize(noiseMap, 0).x);
    return textureLod(noiseMap, p / (side * kSfeTerrainNoiseTexelMetres) + 0.5, 0.0);
}

// How deep in the forest `p` (the terrain's own x/z) is, from VegetationPlacementsComponent::forestMap: centred on
// the terrain, one texel every kSfeForestTexelMetres.
float sfeForest(sampler2D forestMap, vec2 p) {
    float side = float(textureSize(forestMap, 0).x);
    return textureLod(forestMap, p / (side * kSfeForestTexelMetres) + 0.5, 0.0).r;
}

// `p` world x/z in metres, `height` above the water line, `slope` 1 - the normal's y.
// `noise` is sfeTerrainNoise at `p`.
SfeTerrainLayers sfeTerrainLayers(vec2 p, float height, float slope, float forest, vec4 noise) {
    SfeTerrainLayers layers;
    layers.broad = noise.r;
    layers.patches = sfeFbm(p * 0.11 + 13.0, 3);
    float shoreLine = 0.35 + 0.55 * layers.patches;
    layers.underwater = 1.0 - smoothstep(-0.7, -0.15, height);
    layers.wet = 1.0 - smoothstep(-0.15, 0.3, height);
    float bank = 1.0 - smoothstep(shoreLine - 0.6, shoreLine + 0.15, height);
    float sandy = smoothstep(0.5, 0.72, noise.g);
    layers.sand = bank * sandy;
    layers.mud = bank * (1.0 - sandy);
    layers.rock = smoothstep(0.33, 0.6, slope + 0.12 * (layers.patches - 0.5));
    layers.forest = forest;
    // Under the trees the grass gives way to roots, broken by the odd patch of bare earth; ragged where the forest thins.
    float underTrees = smoothstep(0.2, 0.7, forest + 0.25 * (layers.patches - 0.5)) * (1.0 - bank);
    float mossy = smoothstep(0.58, 0.82, layers.patches) * underTrees;
    layers.earth = max(smoothstep(0.6, 0.88, layers.broad * 0.6 + layers.patches * 0.5), mossy);
    layers.litter = underTrees * (1.0 - mossy);
    layers.roadDistance = (noise.a - 0.5) * kSfeRoadTextureRange;
    // A ragged edge where the verge grass creeps in.
    layers.road = 1.0 - smoothstep(1.2, 2.6 + 0.6 * layers.patches, abs(layers.roadDistance));
    return layers;
}

// The linear albedo of the grass, lush where `broad` is low and dry where it is high; `fine` in 0..1 shifts it a little.
vec3 sfeTerrainGrass(float broad, float fine) {
    const vec3 lush = vec3(0.028, 0.062, 0.016);
    const vec3 dry = vec3(0.070, 0.085, 0.030);
    return mix(lush, dry, smoothstep(0.4, 0.8, broad + 0.25 * (fine - 0.5)));
}

#endif

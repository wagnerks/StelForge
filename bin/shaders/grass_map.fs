#version 450 core
#include "sfe.glsl"
#include "sfe_terrain.glsl"
#include "sfe_grass.glsl"
// Where each kind of blade grows and what shade it is, over the whole terrain, from the same rules the terrain is
// painted by. Baked once: r grass, g the broad variation that shades it, b reeds, a lily pads.
layout(location = 0) in vec2 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(SFE_SET(0, 19)) uniform sampler2D forestMap;
layout(SFE_SET(0, 20)) uniform sampler2D terrainNoise;

void main() {
    vec2 local = (TexCoords - 0.5) * grass.terrain.w;
    float slope;
    float height = sfeGrassGround(local, slope);
    SfeTerrainLayers layers = sfeTerrainLayers(local, height, slope, sfeForest(forestMap, local), sfeTerrainNoise(terrainNoise, local));
    // Nothing out of the water, and only the odd blade on the mud. The blades cut the waterline exactly themselves.
    // Under the trees hardly any, but for a few clumps where a gap lets the light down.
    float clearing = smoothstep(0.45, 0.65, layers.patches);
    float grassCover = (1.0 - layers.sand) * (1.0 - 0.95 * layers.mud) * (1.0 - layers.rock) * (1.0 - 0.8 * layers.earth)
        * (1.0 - layers.litter * mix(0.97, 0.6, clearing));
    // Bare wheel tracks on the road, with grass down the strip between them.
    float roadStrip = 1.0 - smoothstep(0.15, 0.35, abs(layers.roadDistance));
    grassCover *= 1.0 - layers.road * (1.0 - 0.35 * roadStrip);
    // Short and thin where the litter lies, so it shows.
    float littered = grass.litter.z * smoothstep(-grass.litter.x - 0.3, -grass.litter.x, height)
        * (1.0 - smoothstep(grass.litter.y, grass.litter.y + 0.4, height));
    grassCover *= 1.0 - 0.75 * littered;
    // Reeds stand in clumps where the bank runs into shallow water, never on a sandy beach.
    float shallow = smoothstep(-0.9, -0.45, height) * (1.0 - smoothstep(0.15, 0.4, height));
    float reeds = shallow * smoothstep(0.5, 0.6, sfeFbm(local * 0.035 + 23.0, 3)) * (1.0 - layers.sand) * (1.0 - layers.rock);
    // Lily pads float in sheltered patches a little further out.
    float deeper = smoothstep(-2.4, -1.9, height) * (1.0 - smoothstep(-0.8, -0.5, height));
    // Loose groups inside each patch, never a carpet.
    float lilies = deeper * smoothstep(0.58, 0.68, sfeFbm(local * 0.025 + 71.0, 3)) * smoothstep(0.45, 0.75, sfeFbm(local * 0.3 + 5.0, 2))
        * 0.45 * (1.0 - layers.sand);
    FragColor = vec4(grassCover, layers.broad, reeds, lilies);
}

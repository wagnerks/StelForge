#ifndef SFE_GRASS_GLSL
#define SFE_GRASS_GLSL
// What GrassPass hands its shaders, and the ground the blades stand on. Include after sfe.glsl.

// Mirrors GrassPass::GrassParams. std140.
layout(SFE_SET(0, 16), std140) uniform GrassBlock {
    vec4 terrain; // xyz the terrain's world position, w its size
    vec4 grid;    // x heights per side, y metres between them, w min terrain height
    vec4 trample; // xyz the character's feet, w how far round them the grass is pushed aside
    vec4 camera;  // xyz
    vec4 litter;  // x how far under the water line and y how far over it the litter lies; z 1 when there is any
} grass;

// The terrain's heights, one texel per mesh vertex, row by row along z.
layout(SFE_SET(0, 17)) uniform sampler2D grassHeights;

// The terrain mesh's height at `local` (x/z in the terrain's space), on the very triangle drawn there, and how
// steep it is there (1 - the normal's y).
float sfeGrassGround(vec2 local, out float slope) {
    int n = int(grass.grid.x + 0.5);
    float step = grass.grid.y;
    float halfSize = grass.terrain.w * 0.5;
    vec2 g = clamp((local + halfSize) / step, vec2(0.0), vec2(float(n - 1) - 1e-3));
    ivec2 c = ivec2(floor(g));
    vec2 f = g - vec2(c);
    float a = texelFetch(grassHeights, c, 0).r;
    float b = texelFetch(grassHeights, c + ivec2(1, 0), 0).r;
    float d = texelFetch(grassHeights, c + ivec2(0, 1), 0).r;
    float e = texelFetch(grassHeights, c + ivec2(1, 1), 0).r;
    float h;
    vec2 gradient;
    // Split along the same diagonal the mesh is.
    if (f.x + f.y <= 1.0) {
        h = a + (b - a) * f.x + (d - a) * f.y;
        gradient = vec2(b - a, d - a) / step;
    } else {
        h = e + (d - e) * (1.0 - f.x) + (b - e) * (1.0 - f.y);
        gradient = vec2(e - d, e - b) / step;
    }
    slope = 1.0 - normalize(vec3(-gradient.x, 1.0, -gradient.y)).y;
    return h;
}

#endif

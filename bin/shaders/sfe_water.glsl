#ifndef SFE_WATER_GLSL
#define SFE_WATER_GLSL
// Include after sfe_sky.glsl.

// Mirrors WaterParams in WaterPass.cpp. std140.
#define SFE_WATER_PARAMS \
    SFE_SKY_PARAMS       \
    vec4 surface;        /* xz centre, y level, w half size */ \
    vec4 look;           /* x clarity in metres, yz 1 / render size */

float sfeCausticLayer(vec2 p, float t) {
    vec2 warp = vec2(sfeValueNoise(p * 0.6 + vec2(t * 0.25, 0.0)), sfeValueNoise(p * 0.6 + vec2(3.7, -t * 0.21))) * 1.8;
    float n = sfeValueNoise(p * 1.9 + warp + vec2(t * 0.15));
    return pow(1.0 - abs(n * 2.0 - 1.0), 8.0);
}

// Two layers of thin ridges in warped noise, drifting against each other: where they cross, the light gathers.
float sfeCaustics(vec2 p, float t) {
    return sfeCausticLayer(p, t) * 0.6 + sfeCausticLayer(p * 1.37 + 11.0, t * 1.3) * 0.6;
}

// How much brighter the waves' focused sunlight makes a point `bottom` under the surface at `level`: a drifting net of
// bright lines, strongest just under the surface and gone `fade` metres down. Found where the sun's ray through the
// point crossed the surface.
float sfeCausticLight(vec3 bottom, float level, vec3 toSun, float seconds, float fade) {
    if (toSun.y <= 0.0) {
        return 1.0;
    }
    float depth = max(level - bottom.y, 0.0);
    float reach = (1.0 - smoothstep(0.3, fade, depth)) * smoothstep(0.02, 0.2, depth);
    if (reach <= 0.0) {
        return 1.0;
    }
    vec2 at = bottom.xz + toSun.xz / max(toSun.y, 0.2) * depth;
    return 1.0 + sfeCaustics(at, seconds) * reach * 2.2 * clamp(toSun.y * 3.0, 0.0, 1.0);
}

// Per metre, how the water takes the light away: a forest lake is stained by what the trees drop into it, blue goes
// first, then green, and a metre or two down only a dark brown-green is left. `clarity` in metres.
vec3 sfeWaterExtinction(float clarity) {
    return vec3(0.55, 0.45, 0.85) * (4.0 / clarity);
}

// What the water itself scatters back towards the eye in place of the light it took away.
vec3 sfeWaterBody(vec3 sun, vec3 toSun, vec3 skyLight) {
    return vec3(0.0025, 0.005, 0.004) * (sun * max(toSun.y, 0.0) + skyLight) / 3.14159265;
}

#endif

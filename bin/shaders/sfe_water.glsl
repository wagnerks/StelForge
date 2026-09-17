#ifndef SFE_WATER_GLSL
#define SFE_WATER_GLSL
// Include after sfe_sky.glsl.

// Mirrors WaterParams in WaterPass.cpp. std140.
#define SFE_WATER_PARAMS \
    SFE_SKY_PARAMS       \
    vec4 surface;        /* xz centre, y level, w half size */ \
    vec4 look;           /* x clarity in metres, yz 1 / render size */

#endif

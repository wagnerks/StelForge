#ifndef SFE_POST_GLSL
#define SFE_POST_GLSL
// The block the post shaders share. Mirrors FinalPass::PostParams. std140.

layout(SFE_SET(0, 15), std140) uniform PostBlock {
    float bloomStrength;
    float exposure;
    float vignette;
    float ca;
    // How hard the sharpening pulls. 0 leaves the image as TAA resolved it; 1 is as far as the filter
    // goes without ringing.
    float sharpness;
    float autoExposure;   // 1 to follow the scene's brightness
    float flareStrength;
    float warmth;         // -1 cool .. 1 warm
    float saturation;
    float contrast;
    float sunU;           // where the sun is on screen, UV
    float sunV;
    float sunInFront;     // 1 when the sun is in front of the camera
    float aspect;         // width over height
    float adaptSeconds;   // since the last frame; very large to adapt at once
    float adaptSpeed;     // how fast the eye follows, per second
    vec4 sunColor;        // rgb radiance of the sun, 0 without an outdoor sky
    vec4 grade;           // x the height of the direction to the sun (1 without a sky), y how much the time of day grades, z film grain, w seconds
    vec4 extra;           // x how strong the sun's rays are
} params;

#endif

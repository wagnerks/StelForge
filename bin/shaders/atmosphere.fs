#version 450 core
#include "sfe.glsl"
#include "sfe_sky.glsl"
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 FragColor;

layout(SFE_SET(0, 20)) uniform sampler2D skyLut;
layout(SFE_SET(0, 21)) uniform sampler2D cloudLayer;

layout(SFE_SET(0, 15), std140) uniform SkyBlock {
    SFE_SKY_PARAMS
} sky;

void main() {
    vec4 far = sky.invViewProj * vec4(vNdc, 0.5, 1.0);
    vec3 dir = normalize(far.xyz / far.w - sky.cameraPos.xyz);
    vec3 color = sfeSky(skyLut, cloudLayer, dir, sky.toSun.xyz, sky.sunColor.rgb, sky.cameraPos.w, sky.toSun.w, sfeCloudDrift(sky.wind, sky.sunColor.w), true, sky.wind.w);
    FragColor = vec4(color, 1.0);
}

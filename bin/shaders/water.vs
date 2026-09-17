#version 450 core
#include "sfe.glsl"
#include "sfe_sky.glsl"
#include "sfe_water.glsl"
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

layout(location = 0) out vec3 vWorld;

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

layout(SFE_SET(0, 15), std140) uniform WaterBlock {
    SFE_WATER_PARAMS
} water;

void main() {
    // The unit quad laid flat at the water level.
    vWorld = vec3(water.surface.x + aPos.x * water.surface.w, water.surface.y, water.surface.z + aPos.y * water.surface.w);
    gl_Position = matrices.PV * vec4(vWorld, 1.0);
}

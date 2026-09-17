#ifndef SFE_IMPOSTOR_GLSL
#define SFE_IMPOSTOR_GLSL
// What ImpostorPass hands its shaders. Include after sfe.glsl.
//
// The atlas holds every model seen from kSfeImpostorViews directions around it, at several sizes: level 0 frames
// fill the left part, model by model in rows and view by view in columns; each coarser level, frames half the
// size, is stacked below the one before in the column right of level 0.

const int kSfeImpostorViews = 8;
const int kSfeImpostorLevels = 5;
// The share of a frame left empty on each side, so filtering never reaches into the next frame.
const float kSfeImpostorMargin = 0.02;

// The baking shaders take the layout above and nothing else.
#ifndef SFE_IMPOSTOR_LAYOUT_ONLY

// Mirrors ImpostorPass::GpuInstance.
struct SfeImpostorInstance {
    vec4 posScale; // xyz where it stands, w its scale
    vec4 yawModel; // x yaw in radians, y the model, z the placement (its bit in entityBits)
};
layout(SFE_SET(0, 20), std430) readonly buffer ImpostorInstances {
    SfeImpostorInstance impostors[];
};
// Bit per placement: set while an entity draws it.
layout(SFE_SET(0, 21), std430) readonly buffer ImpostorEntityBits {
    uint entityBits[];
};
// Per model: x half the frame's side, y its centre's height, at scale 1.
layout(SFE_SET(0, 22), std430) readonly buffer ImpostorModels {
    vec4 impostorModels[];
};

// Mirrors ImpostorPass::DrawParams.
SFE_PUSH_BLOCK ImpostorDraw {
    mat4 viewProj; // light views only; a camera's comes from SharedMatrices
    vec4 eye;      // xyz the camera; for a light, the direction its light travels, and w 1
    vec4 atlas;    // x a level 0 frame's side in texels, y and z the atlas size, w the models
    vec4 params;   // x how far a light's picture is pushed away from it, in half sizes; y texels a metre covers
                   // one metre away (a light: at any distance)
} draw;

// Atlas UV rect (xy corner, zw size) of one frame.
vec4 sfeImpostorFrame(int model, int view, int level) {
    float size = draw.atlas.x / float(1 << level);
    vec2 corner = level == 0 ? vec2(0.0) : vec2(float(kSfeImpostorViews) * draw.atlas.x, 0.0);
    for (int l = 1; l < level; ++l) {
        corner.y += draw.atlas.w * draw.atlas.x / float(1 << l);
    }
    corner += vec2(float(view), float(model)) * size;
    return vec4(corner / draw.atlas.yz, vec2(size) / draw.atlas.yz);
}
#endif

#endif

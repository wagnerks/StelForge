#version 450 core
#include "sfe.glsl"
// One level of the depth pyramid: every texel is the farthest depth under it in the level below (the depth
// texture for level 0). Levels are stored one after another in a buffer, since the RHI has no storage images.
layout(local_size_x = 16, local_size_y = 16) in;

layout(SFE_SET(0, 1)) uniform sampler2D depthTexture;
layout(SFE_SET(0, 2), std430) buffer Pyramid {
    float depths[];
};

SFE_PUSH_BLOCK HizStep {
    uint srcOffset;
    uint srcWidth;
    uint srcHeight;
    uint dstOffset;
    uint dstWidth;
    uint dstHeight;
    uint fromTexture;
    uint _pad;
} step;

void main() {
    uvec2 p = gl_GlobalInvocationID.xy;
    if (p.x >= step.dstWidth || p.y >= step.dstHeight) {
        return;
    }
    // A texel covers two below it, and the last one in a row or column also covers the odd one left over.
    uint w = p.x + 1u == step.dstWidth ? step.srcWidth - p.x * 2u : 2u;
    uint h = p.y + 1u == step.dstHeight ? step.srcHeight - p.y * 2u : 2u;
    float farthest = 0.0;
    for (uint y = 0u; y < h; ++y) {
        for (uint x = 0u; x < w; ++x) {
            uvec2 s = p * 2u + uvec2(x, y);
            float d = step.fromTexture != 0u
                ? texelFetch(depthTexture, ivec2(s), 0).r
                : depths[step.srcOffset + s.y * step.srcWidth + s.x];
            farthest = max(farthest, d);
        }
    }
    depths[step.dstOffset + p.y * step.dstWidth + p.x] = farthest;
}

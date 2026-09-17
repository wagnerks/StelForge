#version 450 core
#include "sfe.glsl"
layout(SFE_SET(0, 0)) uniform sampler2D ssaoInput;
layout(SFE_SET(0, 1)) uniform sampler2D sceneDepth;
layout(SFE_SET(0, 3), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

layout(SFE_SET(0, 2), std140) uniform SSAOBlurParams {
    float sigmaS;
    float sigmaL;
    float facS;
    float facL;
} params;

layout(location = 0) in vec2 TexCoords;
layout(location = 0) out float FragColor;

void main() {
    // In view space: only distances between positions matter, and they are the same there, for one matrix product a tap
    // instead of two.
    bool sky;
    vec3 p0 = sfeSceneViewPosition(sceneDepth, TexCoords, matrices.invProjection, sky);
    float center = texture(ssaoInput, TexCoords).r;
    if (sky) {
        FragColor = 1.0;
        return;
    }

    vec2 texel = 1.0 / vec2(textureSize(ssaoInput, 0));

    // dot(dp, dp) is a raw world distance, so on a floor seen at a grazing angle neighbouring
    // texels are metres apart, every weight underflows to zero and the blur degenerates to the
    // centre sample -- it stops denoising exactly where the noise is worst. Normalising by the
    // local per-texel step makes the range term measure "off the surface" instead of "far away".
    bool skyX;
    bool skyY;
    vec3 dpx = sfeSceneViewPosition(sceneDepth, TexCoords + vec2(texel.x, 0.0), matrices.invProjection, skyX) - p0;
    vec3 dpy = sfeSceneViewPosition(sceneDepth, TexCoords + vec2(0.0, texel.y), matrices.invProjection, skyY) - p0;
    // A neighbour with nothing drawn was the world origin before: its offset in view space is the camera's.
    if (skyX) {
        dpx = matrices.view[3].xyz - p0;
    }
    if (skyY) {
        dpy = matrices.view[3].xyz - p0;
    }
    float step2 = max(min(dot(dpx, dpx), dot(dpy, dpy)), 1e-6);
    float sum = 0.0;
    float wsum = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 uv = TexCoords + vec2(float(x), float(y)) * texel;
            bool tapSky;
            vec3 p = sfeSceneViewPosition(sceneDepth, uv, matrices.invProjection, tapSky);
            if (tapSky) {
                continue;
            }
            vec3 dp = p - p0;
            float d2 = float(x * x + y * y);
            float rel = dot(dp, dp) / (step2 * max(d2, 1.0));
            float w = exp(params.facS * d2 + params.facL * rel);
            sum += texture(ssaoInput, uv).r * w;
            wsum += w;
        }
    }
    FragColor = wsum > 1e-6 ? sum / wsum : center;
}

#version 450 core
#include "sfe.glsl"
layout(SFE_SET(0, 0)) uniform sampler2D ssaoInput;
layout(SFE_SET(0, 1)) uniform sampler2D gPosition;

layout(SFE_SET(0, 2), std140) uniform SSAOBlurParams {
    float sigmaS;
    float sigmaL;
    float facS;
    float facL;
} params;

layout(location = 0) in vec2 TexCoords;
layout(location = 0) out float FragColor;

void main() {
    vec3 p0 = texture(gPosition, TexCoords).rgb;
    float center = texture(ssaoInput, TexCoords).r;
    if (dot(p0, p0) < 1e-8) {
        FragColor = 1.0;
        return;
    }

    vec2 texel = 1.0 / vec2(textureSize(ssaoInput, 0));

    // dot(dp, dp) is a raw world distance, so on a floor seen at a grazing angle neighbouring
    // texels are metres apart, every weight underflows to zero and the blur degenerates to the
    // centre sample -- it stops denoising exactly where the noise is worst. Normalising by the
    // local per-texel step makes the range term measure "off the surface" instead of "far away".
    vec3 dpx = texture(gPosition, TexCoords + vec2(texel.x, 0.0)).rgb - p0;
    vec3 dpy = texture(gPosition, TexCoords + vec2(0.0, texel.y)).rgb - p0;
    float step2 = max(min(dot(dpx, dpx), dot(dpy, dpy)), 1e-6);
    float sum = 0.0;
    float wsum = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 uv = TexCoords + vec2(float(x), float(y)) * texel;
            vec3 p = texture(gPosition, uv).rgb;
            if (dot(p, p) < 1e-8) {
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

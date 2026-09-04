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
            float w = exp(params.facS * float(x * x + y * y) + params.facL * dot(dp, dp));
            sum += texture(ssaoInput, uv).r * w;
            wsum += w;
        }
    }
    FragColor = wsum > 1e-6 ? sum / wsum : center;
}

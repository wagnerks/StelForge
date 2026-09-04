#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D inputTexture;

void main() {
    vec2 texel = 1.0 / vec2(textureSize(inputTexture, 0));
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    const float k[3] = float[](0.227027, 0.316216, 0.070270);
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            float w = k[abs(x) < 3 ? abs(x) : 2] * k[abs(y) < 3 ? abs(y) : 2];
            vec3 c = texture(inputTexture, TexCoords + vec2(x, y) * texel * 2.0).rgb;
            acc += max(c, vec3(0.0)) * w;
            wsum += w;
        }
    }
    vec3 filtered = acc / max(wsum, 1e-4);
    float lum = dot(filtered, vec3(0.2126, 0.7152, 0.0722));
    float knee = 0.6;
    float t = max(lum - 1.15, 0.0);
    float w = t * t / (t + knee);
    FragColor = vec4(filtered * (w / max(lum, 1e-4)), 1.0);
}

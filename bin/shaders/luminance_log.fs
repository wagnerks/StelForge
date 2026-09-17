#version 450 core
#include "sfe.glsl"
// The log of the luminance, averaged over the footprint of each texel of a small target: the first step of the
// frame's log-average brightness.
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D inputTexture;

void main() {
    // One texel of the target, in UV: the full-screen quad's coordinates change by that much per pixel.
    vec2 footprint = vec2(dFdx(TexCoords.x), dFdy(TexCoords.y));
    float sum = 0.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 offset = (vec2(x, y) + 0.5) / 4.0 - 0.5;
            vec3 c = max(texture(inputTexture, TexCoords + offset * footprint).rgb, vec3(0.0));
            sum += log(max(dot(c, vec3(0.2126, 0.7152, 0.0722)), 1e-4));
        }
    }
    FragColor = vec4(sum / 16.0, 0.0, 0.0, 1.0);
}

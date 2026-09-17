#version 450 core
#include "sfe.glsl"
// The average of the log luminance over each texel's footprint in the level above.
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D inputTexture;

void main() {
    vec2 footprint = vec2(dFdx(TexCoords.x), dFdy(TexCoords.y));
    float sum = 0.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 offset = (vec2(x, y) + 0.5) / 4.0 - 0.5;
            sum += texture(inputTexture, TexCoords + offset * footprint).r;
        }
    }
    FragColor = vec4(sum / 16.0, 0.0, 0.0, 1.0);
}

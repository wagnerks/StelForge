#version 450 core
#include "sfe.glsl"

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 gOutlines;

layout(SFE_SET(0, 25)) uniform sampler2D gLightsP;
layout(SFE_SET(0, 26)) uniform sampler2D gDepth;
layout(SFE_SET(0, 27)) uniform sampler2D gOutlinesP;

void main() {
    vec2 texSize = textureSize(gOutlinesP, 0).xy;
    float xStep = 1.0 / texSize.x;
    float yStep = 1.0 / texSize.y;

    float gx = texture(gDepth, TexCoord - vec2(xStep, 0.0)).a * 1.0 +
               texture(gDepth, TexCoord - vec2(xStep, yStep)).a * 2.0 +
               texture(gDepth, TexCoord - vec2(xStep, -yStep)).a * 2.0 -
               texture(gDepth, TexCoord + vec2(xStep, 0.0)).a * 1.0 -
               texture(gDepth, TexCoord + vec2(xStep, yStep)).a * 2.0 -
               texture(gDepth, TexCoord + vec2(xStep, -yStep)).a * 2.0;

    float gy = texture(gDepth, TexCoord - vec2(0.0, yStep)).a * 1.0 +
               texture(gDepth, TexCoord - vec2(xStep, yStep)).a * 2.0 +
               texture(gDepth, TexCoord + vec2(0.0, yStep)).a * 1.0 -
               texture(gDepth, TexCoord - vec2(0.0, -yStep)).a * 1.0 -
               texture(gDepth, TexCoord + vec2(xStep, -yStep)).a * 2.0 -
               texture(gDepth, TexCoord + vec2(0.0, -yStep)).a * 1.0;

    float edge = sqrt(gx * gx + gy * gy);
    float threshold = 7.5;

    gOutlines = vec4(0.0);
    if (edge > threshold) {
        gOutlines.r = 1.0;
    }

    gOutlines.g = texture(gOutlinesP, TexCoord).g;

    int size = 4;
    float separation = 2.0;
    gOutlines.b = 0.0;
    float count = 0.0;

    for (int i = -size; i <= size; ++i) {
        for (int j = -size; j <= size; ++j) {
            vec2 pos = TexCoord + vec2(i, j) / texSize * separation;
            if (pos.x < 0.0 || pos.x > 1.0 || pos.y < 0.0 || pos.y > 1.0) {
                continue;
            }
            gOutlines.b += texture(gOutlinesP, pos).g;
            count += 0.5;
        }
    }

    gOutlines.b /= count;
    if (gOutlines.g > 0.0) {
        gOutlines.b = 0.0;
    }
}

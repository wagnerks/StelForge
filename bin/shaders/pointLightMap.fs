#version 450 core
#include "sfe.glsl"
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in vec2 TexCoords1;

layout(SFE_SET(0, 4)) uniform sampler2D texture_diffuse1;

SFE_PUSH_BLOCK Push {
    int lightCount;
    float casterOpacity;
    float alphaCutoff;
} params;

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

void main() {
    vec4 t = textureLod(texture_diffuse1, TexCoords, 0.0);
    float cutoff = abs(params.alphaCutoff);
    float a = params.alphaCutoff < 0.0 ? max(t.r, t.g) : t.a;
    a = min(a, textureLod(texture_diffuse1, TexCoords1, 0.0).a);
    if (cutoff > 0.0 && a < cutoff) {
        discard;
    }
    float opacity = clamp(params.casterOpacity, 0.0, 1.0);
    if (opacity < 0.999 && ign(gl_FragCoord.xy) > opacity) {
        discard;
    }
}

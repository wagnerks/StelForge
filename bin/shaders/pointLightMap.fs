#version 450 core
#include "sfe.glsl"
SFE_PUSH_BLOCK Push {
    int lightCount;
    float casterOpacity;
} params;

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

void main() {
    float opacity = clamp(params.casterOpacity, 0.0, 1.0);
    if (opacity < 0.999 && ign(gl_FragCoord.xy) > opacity) {
        discard;
    }
}

#version 430 core

layout(std140, binding = 15) uniform PushConstants {
    float casterOpacity;
} params;

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

void main() {
    // Stochastic transparency for shadow maps: PCF averages leftover
    // depth samples into a soft shadow proportional to opacity.
    float opacity = clamp(params.casterOpacity, 0.0, 1.0);
    if (opacity < 0.999 && ign(gl_FragCoord.xy) > opacity) {
        discard;
    }
}

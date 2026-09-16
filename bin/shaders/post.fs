#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D inputTexture;
layout(SFE_SET(0, 1)) uniform sampler2D bloomColor;

layout(SFE_SET(0, 15), std140) uniform PushConstants {
    float bloomStrength;
    float exposure;
    float vignette;
    float ca;
    // How hard the sharpening pulls. 0 leaves the image as TAA resolved it; 1 is as far as the filter
    // below goes without ringing.
    float sharpness;
} params;

vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 tonemapped(vec2 uv) {
    vec3 hdr = texture(inputTexture, uv).rgb + texture(bloomColor, uv).rgb * params.bloomStrength;
    return pow(max(aces(hdr * params.exposure), vec3(0.0)), vec3(1.0 / 2.2));
}

void main() {
    vec2 uv = TexCoords;
    vec2 ca = (uv - 0.5) * params.ca;

    vec3 hdr;
    hdr.r = texture(inputTexture, uv + ca).r;
    hdr.g = texture(inputTexture, uv).g;
    hdr.b = texture(inputTexture, uv - ca).b;
    hdr += texture(bloomColor, uv).rgb * params.bloomStrength;

    vec3 mapped = pow(max(aces(hdr * params.exposure), vec3(0.0)), vec3(1.0 / 2.2));

    // Contrast adaptive sharpening, after the tonemap and clamped to what the four neighbours already
    // contain. The pass used to add a third of an unsharp mask to the HDR values instead: an edge between
    // a bright and a dark sample overshot at both ends, the tonemap turned the overshoot into a coloured
    // fringe, and every pixel of aliasing TAA had just averaged away came back amplified. The amount here
    // falls off where the neighbourhood is already near black or near white, which is where a plain
    // unsharp mask does its worst.
    vec2 texel = 1.0 / vec2(textureSize(inputTexture, 0));
    vec3 north = tonemapped(uv - vec2(0.0, texel.y));
    vec3 south = tonemapped(uv + vec2(0.0, texel.y));
    vec3 west = tonemapped(uv - vec2(texel.x, 0.0));
    vec3 east = tonemapped(uv + vec2(texel.x, 0.0));
    vec3 lo = min(mapped, min(min(north, south), min(west, east)));
    vec3 hi = max(mapped, max(max(north, south), max(west, east)));
    vec3 headroom = clamp(min(lo, 1.0 - hi) / max(hi, vec3(1e-4)), 0.0, 1.0);
    vec3 weight = sqrt(headroom) * (-1.0 / mix(8.0, 5.0, clamp(params.sharpness, 0.0, 1.0)));
    mapped = clamp(((north + south + west + east) * weight + mapped) / (1.0 + 4.0 * weight), lo, hi);

    float satLuma = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
    mapped = mix(vec3(satLuma), mapped, 1.16);

    vec2 p = uv * 2.0 - 1.0;
    float vig = 1.0 - dot(p, p) * params.vignette;
    mapped *= vig;

    FragColor = vec4(mapped, 1.0);
}

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
} params;

vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 uv = TexCoords;
    vec2 ca = (uv - 0.5) * params.ca;

    vec3 hdr;
    hdr.r = texture(inputTexture, uv + ca).r;
    hdr.g = texture(inputTexture, uv).g;
    hdr.b = texture(inputTexture, uv - ca).b;
    hdr += texture(bloomColor, uv).rgb * params.bloomStrength;

    vec2 texel = 1.0 / vec2(textureSize(inputTexture, 0));
    vec3 blur = (
        texture(inputTexture, uv + vec2(texel.x, 0.0)).rgb +
        texture(inputTexture, uv - vec2(texel.x, 0.0)).rgb +
        texture(inputTexture, uv + vec2(0.0, texel.y)).rgb +
        texture(inputTexture, uv - vec2(0.0, texel.y)).rgb) * 0.25;
    hdr += (hdr - blur) * 0.32;

    vec3 mapped = aces(hdr * params.exposure);
    mapped = pow(max(mapped, vec3(0.0)), vec3(1.0 / 2.2));
    float satLuma = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
    mapped = mix(vec3(satLuma), mapped, 1.16);

    vec2 p = uv * 2.0 - 1.0;
    float vig = 1.0 - dot(p, p) * params.vignette;
    mapped *= vig;

    FragColor = vec4(mapped, 1.0);
}

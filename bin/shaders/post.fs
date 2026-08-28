#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D inputTexture;
layout(binding = 1) uniform sampler2D bloomColor;

layout(std140, binding = 15) uniform PushConstants {
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

    vec3 mapped = aces(hdr * params.exposure);
    mapped = pow(max(mapped, vec3(0.0)), vec3(1.0 / 2.2));

    vec2 p = uv * 2.0 - 1.0;
    float vig = 1.0 - dot(p, p) * params.vignette;
    mapped *= vig;

    FragColor = vec4(mapped, 1.0);
}

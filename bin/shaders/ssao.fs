#version 450 core
#include "sfe.glsl"
layout(location = 0) out float FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D gPosition;
layout(SFE_SET(0, 1)) uniform sampler2D gNormal;

layout(SFE_SET(0, 3), std140) uniform SSAOParams {
    float samples;
    float intensity;
    float scale;
    float bias;
    float sample_rad;
    float max_distance;
} params;

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

const int SAMPLE_COUNT = 8;

vec2 vogel(int i, float phi) {
    float r = sqrt(float(i) + 0.5) / sqrt(float(SAMPLE_COUNT));
    float theta = float(i) * 2.399963 + phi;
    return vec2(cos(theta), sin(theta)) * r;
}

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

vec3 toView(vec3 world) {
    return (matrices.view * vec4(world, 1.0)).xyz;
}

void main() {
    vec3 worldP = texture(gPosition, TexCoords).rgb;
    if (dot(worldP, worldP) < 1e-8) {
        FragColor = 1.0;
        return;
    }

    vec3 p = toView(worldP);
    vec3 n = texture(gNormal, TexCoords).xyz;
    float nLen2 = dot(n, n);
    if (nLen2 > 0.25) {
        n = normalize(mat3(matrices.view) * n);
    } else {
        n = cross(dFdx(p), dFdy(p));
        float n2 = dot(n, n);
        if (n2 < 1e-12) {
            FragColor = 1.0;
            return;
        }
        n *= inversesqrt(n2);
    }

    float phi = ign(TexCoords * vec2(1024.0, 768.0)) * 6.2831853;
    float occlusion = 0.0;
    const float uvRad = 0.032;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 uvOff = vogel(i, phi) * uvRad;
        vec3 sampleView = toView(texture(gPosition, TexCoords + uvOff).rgb);
        if (dot(sampleView, sampleView) < 1e-8) {
            continue;
        }
        float dist = abs(p.z - sampleView.z);
        float rangeCheck = 1.0 - smoothstep(params.max_distance * 0.35, params.max_distance, dist);
        occlusion += (sampleView.z >= p.z + params.bias ? 1.0 : 0.0) * rangeCheck;
    }
    FragColor = pow(1.0 - occlusion / float(SAMPLE_COUNT), params.intensity);
}

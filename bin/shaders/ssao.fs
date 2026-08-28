#version 430 core

out float FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;

layout(std140, binding = 3) uniform SSAOParams {
    float samples;
    float intensity;
    float scale;
    float bias;
    float sample_rad;
    float max_distance;
} params;

layout(std140, binding = 5) uniform SharedMatrices {
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
    n *= sign(dot(n, -p));

    float phi = ign(gl_FragCoord.xy) * 6.2831853;
    float uvR = params.sample_rad / max(abs(p.z), 0.5);
    uvR = min(uvR, 0.035);

    float maxD2 = params.max_distance * params.max_distance;
    float ao = 0.0;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 uv = TexCoords + vogel(i, phi) * uvR;
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
            continue;
        }
        vec3 worldS = texture(gPosition, uv).rgb;
        if (dot(worldS, worldS) < 1e-8) {
            continue;
        }
        vec3 s = toView(worldS);
        vec3 v = s - p;
        float d2 = dot(v, v);
        if (d2 > maxD2 || d2 < 1e-8) {
            continue;
        }
        float occ = max(0.0, dot(n, v) - params.bias) / (d2 + 0.05);
        occ *= 1.0 - smoothstep(maxD2 * 0.35, maxD2, d2);
        ao += occ;
    }

    ao = 1.0 - ao * params.intensity / float(SAMPLE_COUNT);
    FragColor = clamp(ao, 0.0, 1.0);
}

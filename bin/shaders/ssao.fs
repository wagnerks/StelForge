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
    float frameIndex;
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

// Temporal IGN: the golden-ratio shift per frame keeps the pattern decorrelated over time,
// so TAA averages it away instead of leaving it standing in screen space.
float ign(vec2 p, float frame) {
    p += 5.588238 * frame;
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
        // The derivative cross product has no defined orientation, and now that the tangent-plane
        // test depends on the sign, a flipped normal would invert the AO. In view space the eye is
        // at the origin, so a front-facing normal must point back along p.
        if (dot(n, p) > 0.0) {
            n = -n;
        }
    }

    float phi = ign(gl_FragCoord.xy, params.frameIndex) * 6.2831853;
    float occlusion = 0.0;
    // sample_rad is a view-space radius: project it so the disc shrinks with distance
    // instead of covering a fixed 3.2% of the screen at every depth.
    float invZ = 1.0 / max(-p.z, 0.05);
    vec2 uvRad = min(0.5 * params.sample_rad * invZ * abs(vec2(matrices.projection[0][0], matrices.projection[1][1])), vec2(0.006));
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 uvOff = vogel(i, phi) * uvRad;
        vec3 sampleView = toView(texture(gPosition, TexCoords + uvOff).rgb);
        if (dot(sampleView, sampleView) < 1e-8) {
            continue;
        }
        // Occlusion is "the sample sits above my tangent plane", not "the sample is nearer the
        // camera". A pure depth compare makes any plane seen at an angle occlude itself -- the
        // whole floor gets AO that varies with distance, which reads as bands radiating from the
        // camera. n was already computed above and then never used; this is what it is for.
        vec3 diff = sampleView - p;
        float dist = length(diff);
        vec3 dir = diff / max(dist, 1e-5);
        float rangeCheck = 1.0 - smoothstep(params.max_distance * 0.35, params.max_distance, dist);
        occlusion += max(dot(n, dir) - params.bias, 0.0) * rangeCheck;
    }
    FragColor = pow(1.0 - occlusion / float(SAMPLE_COUNT), params.intensity);
}

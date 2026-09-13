#version 450 core
#include "sfe.glsl"

// Computes a set of floats per pixel from the pixel coordinate and the push block alone, with the
// same operations the engine's shaders use, and writes the exact bits of the one picked by
// select.x across RGBA8. Quantity 15 is the pixel id, so readback can be keyed by it rather than by
// row order.
SFE_PUSH_BLOCK FloatBitsPush {
    mat4 viewProj;
    vec4 seed;
    vec4 weights;
    vec4 uvScaleBias;
    vec4 select;
} pc;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

const int kQuantities = 16;

// Entries depend on base at run time, so nothing here is folded by either compiler front end.
mat4 makeMatrix(float base, float k) {
    mat4 m = mat4(0.0);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 3; ++r) {
            m[c][r] = base * (0.113 + 0.071 * float(c) - 0.037 * float(r)) + k * float(c * 4 + r + 1);
        }
        m[c][3] = c == 3 ? 1.0 + base * 0.0123 : base * 0.0021 * float(c + 1);
    }
    return m;
}

float hash(vec2 p) {
    return sfeHash12(p);
}

void main() {
    uint px = uint(gl_FragCoord.x);
    uint py = uint(gl_FragCoord.y);
    float x = float(px);
    float y = float(py);
    float t = (x + y * 64.0) * pc.seed.x + pc.seed.y;

    // Skinning chain, as in g_buffer.vs: weighted bone sum, divide by w, model, view-projection.
    mat4 BoneTransform = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        mat4 bone = makeMatrix(t + float(i) * pc.seed.z, pc.seed.w);
        BoneTransform += bone * (pc.weights[i] * (1.0 + 0.01 * x));
    }
    mat4 model = makeMatrix(t * 0.5 - 1.0, pc.seed.w * 2.0);
    vec4 aPos = vec4(x * 0.37 - 3.0, y * 0.21 + 1.0, t * 0.05, 1.0);
    vec4 newPos = BoneTransform * aPos;
    newPos /= newPos.w;
    vec4 worldPos = model * newPos;
    vec4 clip = pc.viewProj * worldPos;
    vec3 ndc = clip.xyz / clip.w;

    vec3 n = normalize(worldPos.xyz);
    vec3 m = normalize(vec3(aPos.y, t, 1.0));
    float crossDot = dot(cross(n, m), worldPos.xyz);

    // sfeMaskedAlbedo-style selection over five synthetic samples.
    vec3 bestRgb = vec3(0.0);
    float best = -1.0e9;
    for (int i = 0; i < 5; ++i) {
        vec2 o = vec2(x + float(i) * 0.25, y - float(i) * 0.5);
        vec4 s = vec4(hash(o), hash(o + 1.7), hash(o + 3.1), hash(o + 5.3));
        float sat = max(max(s.r, s.g), s.b) - min(min(s.r, s.g), s.b);
        float score = s.a - sat;
        if (score > best) {
            best = score;
            bestRgb = s.rgb;
        }
    }

    float v[kQuantities];
    v[0] = ndc.x;
    v[1] = ndc.y;
    v[2] = ndc.z;
    v[3] = n.x;
    v[4] = crossDot;
    v[5] = pow(fract(t * 0.013) + 0.05, 2.4);
    v[6] = exp(-abs(t) * 0.001);
    v[7] = sqrt(abs(worldPos.x) + 1.0);
    v[8] = mix(ndc.x, ndc.y, fract(t * 0.017));
    v[9] = smoothstep(0.03, 0.10, fract(t * 0.0071) * 0.13);
    v[10] = hash(vec2(x, y));
    v[11] = best;
    v[12] = bestRgb.r;
    v[13] = vUv.x;
    v[14] = vUv.y;
    v[15] = 0.0;

    int q = int(pc.select.x);
    uint b = q == kQuantities - 1 ? py * 65536u + px : floatBitsToUint(v[q]);
    uvec4 bytes = uvec4(b & 255u, (b >> 8u) & 255u, (b >> 16u) & 255u, b >> 24u);
    outColor = vec4(bytes) / 255.0;
}

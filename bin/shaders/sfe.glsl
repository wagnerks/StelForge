#ifndef SFE_GLSL
#define SFE_GLSL

#ifdef VULKAN
#define SFE_SET(setIdx, bindingIdx) set = setIdx, binding = bindingIdx
#define SFE_PUSH_BLOCK layout(push_constant) uniform
#define SFE_INSTANCE_ID gl_InstanceIndex
#define SFE_VERTEX_ID gl_VertexIndex
#else
#extension GL_ARB_shader_draw_parameters : require
#define SFE_SET(setIdx, bindingIdx) binding = bindingIdx
#define SFE_PUSH_BLOCK layout(std140, binding = 15) uniform
// gl_InstanceID starts at 0 whatever firstInstance is; gl_InstanceIndex on Vulkan includes it.
#define SFE_INSTANCE_ID (gl_InstanceID + gl_BaseInstanceARB)
#define SFE_VERTEX_ID gl_VertexID
#endif

// A hash that gives the same bits on every compiler path: integer arithmetic only. The usual
// fract(sin(x) * 43758.5453) does not -- sin() of a large argument differs in its low bits between the
// OpenGL driver's GLSL compiler and the SPIR-V path, and the multiply turns that into another value.
// Quantises p to 1/256 before hashing; mark the expression that builds p `precise` at the call site.
float sfeHash12(vec2 p) {
    uvec2 v = uvec2(ivec2(floor(p * 256.0)));
    v = v * 1664525u + 1013904223u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v ^= v >> 16u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v ^= v >> 16u;
    return float(v.x >> 8u) / 16777216.0;
}

vec4 sfeGBufferPbr(float packedA) {
    return unpackUnorm4x8(floatBitsToUint(packedA));
}

float sfePackGBufferPbr(float metallic, float specFactor, float clearcoat, float clearcoatRough) {
    return uintBitsToFloat(packUnorm4x8(vec4(metallic, specFactor, clearcoat, clearcoatRough)));
}

// Cutout atlases often store leftover RGB in discarded texels. Bilinear mixes that
// into visible edges; snap RGB to the texel center so only coverage uses filtering.
vec3 sfeMaskedAlbedo(sampler2D tex, vec2 uv) {
    vec2 texel = 1.0 / vec2(textureSize(tex, 0));
    vec2 o[5] = vec2[](vec2(0.0), vec2(texel.x, 0.0), vec2(-texel.x, 0.0), vec2(0.0, texel.y), vec2(0.0, -texel.y));
    vec3 bestRgb = vec3(0.0);
    float best = -1.0e9;
    for (int i = 0; i < 5; ++i) {
        vec4 s = textureLod(tex, uv + o[i], 0.0);
        float sat = max(max(s.r, s.g), s.b) - min(min(s.r, s.g), s.b);
        float score = s.a - sat;
        if (score > best) {
            best = score;
            bestRgb = s.rgb;
        }
    }
    return bestRgb;
}

float sfeSkinSss(vec3 albedo, float metallic) {
    float luma = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
    float warm = clamp((albedo.r - albedo.g) * 3.5, 0.0, 1.0);
    warm *= smoothstep(0.03, 0.10, luma);
    warm *= smoothstep(0.0, 0.04, albedo.r - albedo.b);
    return (1.0 - metallic) * warm;
}

#endif

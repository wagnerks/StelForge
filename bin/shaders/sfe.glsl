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

// Jimenez's interleaved gradient noise, fract(52.9829189 * fract(dot(p + 5.588238 * frame, vec2(0.06711056,
// 0.00583715)))), in fixed point. The float form came out different on OpenGL and Vulkan for the same pixel --
// the two shader compilers evaluate it differently even marked precise -- so everything it rotates differed
// between backends. Integer arithmetic has one result: the inner fract keeps 16 bits (the frame shift adds
// frame * 5.588238 * (0.06711056 + 0.00583715) = frame * 26716 / 2^16), and the outer multiply wraps modulo
// 2^32, which is exactly the outer fract.
float sfeInterleavedGradientNoise(vec2 p, uint frame) {
    uvec2 pixel = uvec2(p);
    uint inner = (pixel.x * 4398u + pixel.y * 383u + frame * 26716u) & 0xFFFFu; // fract(dot(p, k)) * 2^16
    uint outer = inner * 3472289u;                                              // fract(52.9829189 * inner / 2^16) * 2^32
    return float(outer >> 8u) / 16777216.0;
}

// vec2(cos(a), sin(a)) without the built-ins, which differ in their low bits between the OpenGL driver's
// GLSL compiler and the SPIR-V path. a is folded to [-pi/4, pi/4] around the nearest quarter turn, where
// Taylor series to the 9th power are within 3e-8, far below a pixel for any disc radius in use.
vec2 sfeCosSin(float a) {
    const float halfPi = 1.5707963267948966;
    precise float q = floor(a / halfPi + 0.5);
    precise float r = a - q * halfPi;
    precise float r2 = r * r;
    precise float s = r * (1.0 - r2 / 6.0 * (1.0 - r2 / 20.0 * (1.0 - r2 / 42.0 * (1.0 - r2 / 72.0))));
    precise float c = 1.0 - r2 / 2.0 * (1.0 - r2 / 12.0 * (1.0 - r2 / 30.0 * (1.0 - r2 / 56.0)));
    int quadrant = int(q) & 3;
    if (quadrant == 1) {
        return vec2(-s, c);
    }
    if (quadrant == 2) {
        return vec2(-c, -s);
    }
    if (quadrant == 3) {
        return vec2(s, -c);
    }
    return vec2(c, s);
}

vec4 sfeGBufferPbr(float packedA) {
    return unpackUnorm4x8(floatBitsToUint(packedA));
}

float sfePackGBufferPbr(float metallic, float specFactor, float clearcoat, float clearcoatRough) {
    return uintBitsToFloat(packUnorm4x8(vec4(metallic, specFactor, clearcoat, clearcoatRough)));
}




// Rec. 709 luminance, the weighting every pass here already used inline.
float sfeLuma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float sfeSkinSss(vec3 albedo, float metallic) {
    float luma = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
    float warm = clamp((albedo.r - albedo.g) * 3.5, 0.0, 1.0);
    warm *= smoothstep(0.03, 0.10, luma);
    warm *= smoothstep(0.0, 0.04, albedo.r - albedo.b);
    return (1.0 - metallic) * warm;
}

// Where a pixel moved since last frame, in UV: history belongs at TexCoords - velocity. Both clip positions
// come from unjittered matrices, so a still surface gives exactly zero.
vec2 sfeVelocity(vec4 clipNow, vec4 clipBefore) {
    if (clipNow.w <= 1e-6 || clipBefore.w <= 1e-6) {
        return vec2(0.0);
    }
    vec2 now = clipNow.xy / clipNow.w;
    vec2 before = clipBefore.xy / clipBefore.w;
    return (now - before) * 0.5;
}

#endif

// One definition of the per-frame matrices block. OpenGL links every shader of a program together and
// rejects a uniform block whose members differ between them, so the body lives here and each shader only
// picks its own set/binding.
#define SFE_SHARED_MATRICES \
    mat4 projection; \
    mat4 view; \
    mat4 PV; \
    mat4 PVNoJitter; \
    mat4 prevPVNoJitter;

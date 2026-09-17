#version 450 core
#include "sfe.glsl"
#include "sfe_grass.glsl"
// Lily pads and their flowers (grass.vs, kind 2). vPad.z is the part: 0 a pad, with xy its own position in the unit
// disc; 1 and 2 an outer and an inner petal, with xy along it and across it; 3 the flower's yellow middle.
layout(location = 0) in vec3 vWorld;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec4 vClip;
layout(location = 4) in vec4 vPrevClip;
layout(location = 5) in vec3 vPad;

layout(location = 0) out vec4 gMaterial;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;
layout(location = 3) out vec2 gVelocity;

const float PI = 3.14159265;

void main() {
    int part = int(vPad.z + 0.5);
    vec3 N = normalize(vNormal);
    vec3 albedo = vColor;
    float roughness = 0.45;
    float specular = 0.35;

    if (part == 0) {
        vec2 p = vPad.xy;
        float r = length(p);
        float angle = atan(p.y, p.x);
        // Not quite round, and the notch a wedge that opens from the middle out.
        float rim = 1.0 - 0.035 * (0.5 + 0.5 * sin(angle * 5.0 + 1.3) * sin(angle * 3.0 - 0.7));
        if (r > rim || (abs(angle) < 0.02 + 0.13 * smoothstep(0.0, 0.35, r) && r > 0.03)) {
            discard;
        }
        // Veins running out from the middle, paler than the leaf, fading towards the rim; blotches over the surface.
        float veinAngle = angle / (2.0 * PI) * 13.0;
        float vein = (1.0 - smoothstep(0.0, 0.07, abs(fract(veinAngle) - 0.5))) * smoothstep(0.05, 0.2, r) * (1.0 - smoothstep(0.6, 0.95, r));
        float blotch = sfeValueNoise(vWorld.xz * 7.0) * 0.6 + sfeValueNoise(vWorld.xz * 19.0) * 0.4;
        albedo *= mix(0.8, 1.2, blotch);
        albedo = mix(albedo, albedo * vec3(1.35, 1.4, 1.1), vein * 0.5);
        // The curled edge shows the reddish underside.
        albedo = mix(albedo, vec3(0.055, 0.025, 0.015), smoothstep(0.94, 1.0, r / rim) * 0.45);
        // Waxy rather than glossy: smoother, a pad against the light turned into a pale mirror of the sky.
        roughness = 0.8;
        specular = 0.08;
    }
    else if (part <= 2) {
        // A pointed petal, widest a third of the way along.
        float u = vPad.x;
        float halfWidth = pow(sin(PI * clamp(u, 0.0, 1.0)), 0.7) * (1.0 - 0.35 * u);
        if (abs(vPad.y) > halfWidth) {
            discard;
        }
        albedo *= 1.0 - 0.08 * (1.0 - smoothstep(0.0, 0.15, abs(abs(vPad.y) - 0.0)));
        if (dot(N, grass.camera.xyz - vWorld) < 0.0) {
            N = -N;
        }
        roughness = 0.6;
        specular = 0.2;
    }
    else {
        // The middle: a round tuft of stamens.
        float r = length(vPad.xy);
        if (r > 1.0) {
            discard;
        }
        float stamens = sfeValueNoise(vPad.xy * 9.0);
        albedo = mix(vec3(0.30, 0.20, 0.02), vColor, smoothstep(0.2, 0.7, stamens));
        roughness = 0.7;
        specular = 0.15;
    }

    gMaterial = sfeGBufferMaterial(0.0, specular, 0.0, 0.04);
    gNormal = vec4(N, gl_FragCoord.z / gl_FragCoord.w);
    gAlbedoSpec = vec4(albedo, roughness);
    gVelocity = sfeVelocity(vClip, vPrevClip);
}

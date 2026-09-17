#version 450 core
#include "sfe.glsl"
#include "sfe_grass.glsl"
// A grass clump into the G-buffer: the kit atlas cut out by its alpha, lit from whichever side the camera sees it with
// the normal leant towards the sky, and marked as letting light through for the lighting to show it against the sun.
layout(location = 0) in vec3 vWorld;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec4 vClip;
layout(location = 4) in vec4 vPrevClip;
layout(location = 5) flat in float vShade;

layout(location = 0) out vec4 gMaterial;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;
layout(location = 3) out vec2 gVelocity;

layout(SFE_SET(0, 22)) uniform sampler2D atlas;

void main() {
    vec4 colour = texture(atlas, vUv);
    if (colour.a < 0.5) {
        discard;
    }
    vec3 N = normalize(vNormal);
    if (dot(N, grass.camera.xyz - vWorld) < 0.0) {
        N = -N;
    }
    N = normalize(mix(N, vec3(0.0, 1.0, 0.0), 0.35));

    gMaterial = sfeGBufferMaterial(0.0, 0.25, 0.0, kSfeTranslucentFoliage);
    gNormal = vec4(N, gl_FragCoord.z / gl_FragCoord.w);
    gAlbedoSpec = vec4(pow(colour.rgb, vec3(2.2)) * vShade, 0.8);
    gVelocity = sfeVelocity(vClip, vPrevClip);
}

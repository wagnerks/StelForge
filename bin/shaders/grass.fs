#version 450 core
#include "sfe.glsl"
#include "sfe_grass.glsl"
// A reed leaf, stem or cattail into the G-buffer, lit from whichever side the camera sees it and leaning its normal
// towards the sky, and letting the sun through.
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

void main() {
    vec3 N = normalize(vNormal);
    if (dot(N, grass.camera.xyz - vWorld) < 0.0) {
        N = -N;
    }
    N = normalize(mix(N, vec3(0.0, 1.0, 0.0), 0.45));

    gMaterial = sfeGBufferMaterial(0.0, 0.3, 0.0, kSfeTranslucentFoliage);
    gNormal = vec4(N, gl_FragCoord.z / gl_FragCoord.w);
    gAlbedoSpec = vec4(vColor, 0.85);
    gVelocity = sfeVelocity(vClip, vPrevClip);
}
